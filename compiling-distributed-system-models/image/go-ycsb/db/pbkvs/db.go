package pbkvs

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"log"
	"strconv"

	"github.com/DistCompiler/pgo/systems/pbkvs/bootstrap"
	"github.com/DistCompiler/pgo/systems/pbkvs/configs"
	"github.com/magiconair/properties"
	"github.com/pingcap/go-ycsb/pkg/ycsb"
)

type thread struct {
	client *bootstrap.Client
	errCh  chan error
}

type pbkvs struct {
	config  configs.Root
	useInts bool

	threads []*thread
}

func (p *pbkvs) ToSqlDB() *sql.DB {
	return nil
}

func (p *pbkvs) Close() error {
	return nil
}

type threadIdxTag struct{}

func (p *pbkvs) InitThread(ctx context.Context, threadIdx int, threadCount int) context.Context {
	if threadCount > p.config.NumClients {
		panic("threadCount cannot be larger than numClients")
	}

	clientId := threadIdx + 1
	client := bootstrap.NewClient(clientId, p.config)
	errCh := make(chan error)

	thread := &thread{
		client: client,
		errCh:  errCh,
	}

	p.threads = append(p.threads, thread)

	go func() {
		errCh <- client.Run()
	}()

	return context.WithValue(ctx, threadIdxTag{}, thread)
}

func (p *pbkvs) CleanupThread(ctx context.Context) {
	thread := ctx.Value(threadIdxTag{}).(*thread)

	if err := thread.client.Close(); err != nil {
		log.Println(err)
	}
	if err := <-thread.errCh; err != nil {
		log.Println(err)
	}
}

func (p *pbkvs) Read(ctx context.Context, table string, key string, fields []string) (map[string][]byte, error) {
	thread := ctx.Value(threadIdxTag{}).(*thread)

	reqKey := table + "/" + key
	resp, err := thread.client.Get(reqKey)
	if err != nil {
		return nil, err
	}

	if p.useInts {
		return make(map[string][]byte), nil
	}

	data := make(map[string][]byte, len(fields))
	err = json.Unmarshal([]byte(resp), &data)
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

func (p *pbkvs) Scan(_ context.Context, _ string, _ string, _ int, _ []string) ([]map[string][]byte, error) {
	return nil, fmt.Errorf("raftkvs doesn't implement key scan")
}

func (p *pbkvs) Update(ctx context.Context, table string, key string, values map[string][]byte) error {
	result, err := p.Read(ctx, table, key, nil)
	if err != nil {
		return err
	}
	for k := range values {
		result[k] = values[k]
	}
	return p.Insert(ctx, table, key, result)
}

func (p *pbkvs) Insert(ctx context.Context, table string, key string, values map[string][]byte) error {
	thread := ctx.Value(threadIdxTag{}).(*thread)

	valuesBytes, err := json.Marshal(values)
	if err != nil {
		return err
	}

	var content string
	if p.useInts {
		content = strconv.Itoa(len(valuesBytes))
	} else {
		content = string(valuesBytes)
	}

	reqKey := table + "/" + key
	reqValue := content

	_, err = thread.client.Put(reqKey, reqValue)
	return err
}

func (p *pbkvs) Delete(ctx context.Context, table string, key string) error {
	return p.Insert(ctx, table, key, make(map[string][]byte))
}

type pbkvsCreator struct{}

const (
	pbkvsConfigPath = "pbkvs.config"
	pbkvsUseInts    = "ycsb.useints"
)

func (rc pbkvsCreator) Create(props *properties.Properties) (ycsb.DB, error) {
	configPath, ok := props.Get(pbkvsConfigPath)
	if !ok {
		return nil, fmt.Errorf("%v is missing", pbkvsConfigPath)
	}

	c, err := configs.ReadConfig(configPath)
	if err != nil {
		return nil, err
	}

	return &pbkvs{
		config:  c,
		useInts: props.GetBool(pbkvsUseInts, false),
	}, nil
}

func init() {
	ycsb.RegisterDBCreator("pbkvs", pbkvsCreator{})
}
