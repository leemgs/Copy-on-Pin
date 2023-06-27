package raftkvs

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"log"
	"strconv"

	"github.com/DistCompiler/pgo/systems/raftkvs/bootstrap"
	"github.com/DistCompiler/pgo/systems/raftkvs/configs"
	"github.com/magiconair/properties"
	"github.com/pingcap/go-ycsb/pkg/ycsb"
)

type thread struct {
	client *bootstrap.Client
	reqCh  chan bootstrap.Request
	respCh chan bootstrap.Response

	errCh chan error
}

type raftKVS struct {
	config  configs.Root
	useInts bool

	threads []*thread
}

func newRaftKVS(c configs.Root) *raftKVS {
	return &raftKVS{config: c}
}

func (r *raftKVS) ToSqlDB() *sql.DB {
	return nil
}

func (r *raftKVS) Close() error {
	return nil
}

type threadIdxTag struct{}

func (r *raftKVS) InitThread(ctx context.Context, threadIdx int, threadCount int) context.Context {
	if threadCount > r.config.NumClients {
		panic("threadCount cannot be larger than numClients")
	}

	clientId := threadIdx + 1
	client := bootstrap.NewClient(clientId, r.config)

	reqCh := make(chan bootstrap.Request)
	respCh := make(chan bootstrap.Response)
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

func (r *raftKVS) CleanupThread(ctx context.Context) {
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

func (r *raftKVS) Read(ctx context.Context, table string, key string, fields []string) (map[string][]byte, error) {
	thread := ctx.Value(threadIdxTag{}).(*thread)

	req := bootstrap.GetRequest{
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

func (r *raftKVS) Scan(_ context.Context, _ string, _ string, _ int, _ []string) ([]map[string][]byte, error) {
	return nil, fmt.Errorf("raftkvs doesn't implement key scan")
}

func (r *raftKVS) Update(ctx context.Context, table string, key string, values map[string][]byte) error {
	result, err := r.Read(ctx, table, key, nil)
	if err != nil {
		return err
	}
	for k := range values {
		result[k] = values[k]
	}
	return r.Insert(ctx, table, key, result)
}

func (r *raftKVS) Insert(ctx context.Context, table string, key string, values map[string][]byte) error {
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

	req := bootstrap.PutRequest{
		Key:   table + "/" + key,
		Value: content,
	}
	thread.reqCh <- req

	<-thread.respCh

	return nil
}

func (r *raftKVS) Delete(ctx context.Context, table string, key string) error {
	return r.Insert(ctx, table, key, make(map[string][]byte))
}

type raftkvsCreator struct{}

const (
	raftkvsConfigPath = "raftkvs.config"
	raftkvsUseInts    = "ycsb.useints"
)

func (rc raftkvsCreator) Create(props *properties.Properties) (ycsb.DB, error) {
	configPath, ok := props.Get(raftkvsConfigPath)
	if !ok {
		return nil, fmt.Errorf("%v is missing", raftkvsConfigPath)
	}

	c, err := configs.ReadConfig(configPath)
	if err != nil {
		return nil, err
	}

	return &raftKVS{
		config:  c,
		useInts: props.GetBool(raftkvsUseInts, false),
	}, nil
}

func init() {
	ycsb.RegisterDBCreator("raftkvs", raftkvsCreator{})
}
