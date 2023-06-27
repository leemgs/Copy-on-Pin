package raftres

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"log"
	"strconv"

	"github.com/DistCompiler/pgo/systems/raftres/configs"
	"github.com/DistCompiler/pgo/systems/raftres/kv"
	"github.com/magiconair/properties"
	"github.com/pingcap/go-ycsb/pkg/ycsb"
)

type thread struct {
	client *kv.Client
	reqCh  chan kv.Request
	respCh chan kv.Response

	errCh chan error
}

type raftRes struct {
	config  configs.Root
	useInts bool

	threads []*thread
}

func newRaftRes(c configs.Root) *raftRes {
	return &raftRes{config: c}
}

func (r *raftRes) ToSqlDB() *sql.DB {
	return nil
}

func (r *raftRes) Close() error {
	return nil
}

type threadIdxTag struct{}

func (r *raftRes) InitThread(ctx context.Context, threadIdx int, threadCount int) context.Context {
	if threadCount > r.config.NumClients {
		panic("threadCount cannot be larger than numClients")
	}

	clientId := threadIdx + 1
	client := kv.NewClient(clientId, r.config)

	reqCh := make(chan kv.Request)
	respCh := make(chan kv.Response)
	errCh := make(chan error)
	thread := &thread{
		client: client,
		reqCh:  reqCh,
		respCh: respCh,
		errCh:  errCh,
	}

	r.threads = append(r.threads, thread)

	go func() {
		errCh <- client.Run(reqCh, respCh)
	}()

	return context.WithValue(ctx, threadIdxTag{}, thread)
}

func (*raftRes) CleanupThread(ctx context.Context) {
	thread := ctx.Value(threadIdxTag{}).(*thread)

	close(thread.reqCh)
	close(thread.respCh)
	if err := thread.client.Close(); err != nil {
		log.Println(err)
	}
	if err := <-thread.errCh; err != nil {
		log.Println(err)
	}
}

func (r *raftRes) Read(ctx context.Context, table string, key string, fields []string) (map[string][]byte, error) {
	thread := ctx.Value(threadIdxTag{}).(*thread)

	req := kv.GetRequest{
		Key: table + "/" + key,
	}
	thread.reqCh <- req

	resp := <-thread.respCh
	if !resp.OK {
		log.Println("key not found")
		return nil, fmt.Errorf("key not found")
	}

	if r.useInts {
		return make(map[string][]byte), nil
	}

	data := make(map[string][]byte, len(fields))
	err := json.Unmarshal([]byte(resp.Value), &data)
	if err != nil {
		log.Printf("json error: %v", err)
		return nil, err
	}

	if len(fields) != 0 {
		fieldsToInclude := make(map[string]bool)
		for _, field := range fields {
			fieldsToInclude[field] = true
		}
		for k := range data {
			if !fieldsToInclude[k] {
				delete(data, k)
			}
		}
	}
	return data, nil
}

func (r *raftRes) Scan(_ context.Context, _ string, _ string, _ int, _ []string) ([]map[string][]byte, error) {
	return nil, fmt.Errorf("raftres doesn't implement key scan")
}

func (r *raftRes) Update(ctx context.Context, table string, key string, values map[string][]byte) error {
	result, err := r.Read(ctx, table, key, nil)
	if err != nil {
		return err
	}
	for k := range values {
		result[k] = values[k]
	}
	return r.Insert(ctx, table, key, result)
}

func (r *raftRes) Insert(ctx context.Context, table string, key string, values map[string][]byte) error {
	thread := ctx.Value(threadIdxTag{}).(*thread)

	valuesBytes, err := json.Marshal(values)
	if err != nil {
		return err
	}

	var content string
	if r.useInts {
		content = strconv.Itoa(len(valuesBytes))
	} else {
		content = string(valuesBytes)
	}

	req := kv.PutRequest{
		Key:   table + "/" + key,
		Value: content,
	}
	thread.reqCh <- req

	<-thread.respCh

	return nil
}

func (r *raftRes) Delete(ctx context.Context, table string, key string) error {
	return r.Insert(ctx, table, key, make(map[string][]byte))
}

type raftresCreator struct{}

const (
	raftresConfigPath = "raftres.config"
	raftresUseInts    = "ycsb.useints"
)

func (rc raftresCreator) Create(props *properties.Properties) (ycsb.DB, error) {
	configPath, ok := props.Get(raftresConfigPath)
	if !ok {
		return nil, fmt.Errorf("%v is missing", raftresConfigPath)
	}

	c, err := configs.ReadConfig(configPath)
	if err != nil {
		return nil, err
	}

	return &raftRes{
		config:  c,
		useInts: props.GetBool(raftresUseInts, false),
	}, nil
}

func init() {
	ycsb.RegisterDBCreator("raftres", raftresCreator{})
}
