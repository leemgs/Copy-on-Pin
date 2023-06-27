package redis

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	goredis "github.com/go-redis/redis/v8"
	"github.com/magiconair/properties"
	"github.com/pingcap/go-ycsb/pkg/ycsb"
)

type redis struct {
	client      *goredis.Client
	numReplicas int
}

func (r *redis) ToSqlDB() *sql.DB {
	return nil
}

func (r *redis) Close() error {
	return r.client.Close()
}

func (r *redis) InitThread(ctx context.Context, _ int, _ int) context.Context {
	return ctx
}

func (r *redis) CleanupThread(_ context.Context) {
}

func (r *redis) Read(ctx context.Context, table string, key string, fields []string) (map[string][]byte, error) {
	data := make(map[string][]byte, len(fields))

	res, err := r.client.Get(ctx, table+"/"+key).Result()
	if err != nil {
		return nil, err
	}

	err = json.Unmarshal([]byte(res), &data)
	if err != nil {
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

func (r *redis) Scan(_ context.Context, _ string, _ string, _ int, _ []string) ([]map[string][]byte, error) {
	return nil, fmt.Errorf("scan is not supported")
}

func (r *redis) Update(ctx context.Context, table string, key string, values map[string][]byte) error {
	d, err := r.Read(ctx, table, key, nil)
	if err != nil {
		return err
	}

	for k := range values {
		d[k] = values[k]
	}

	return r.Insert(ctx, table, key, d)
}

func (r *redis) Insert(ctx context.Context, table string, key string, values map[string][]byte) error {
	data, err := json.Marshal(values)
	if err != nil {
		return err
	}

	pp := r.client.Pipeline()
	_ = pp.Set(ctx, table+"/"+key, string(data), 0)
	_ = pp.Do(ctx, "WAIT", r.numReplicas, 0)
	_, err = pp.Exec(ctx)
	return err
}

func (r *redis) Delete(ctx context.Context, table string, key string) error {
	pp := r.client.Pipeline()
	_ = pp.Del(ctx, table+"/"+key)
	_ = pp.Do(ctx, "WAIT", r.numReplicas, 0)
	_, err := pp.Exec(ctx)
	return err
}

type redisCreator struct{}

func (r redisCreator) Create(p *properties.Properties) (ycsb.DB, error) {
	addr, ok := p.Get(redisAddr)
	if !ok {
		return nil, fmt.Errorf("%s must be specified", redisAddr)
	}
	numSlaves := p.GetInt(redisNumReplicas, -1)
	if numSlaves == -1 {
		return nil, fmt.Errorf("%s must be specified", redisNumReplicas)
	}

	client := goredis.NewClient(&goredis.Options{
		Addr: addr,
	})
	return &redis{
		client:      client,
		numReplicas: numSlaves,
	}, nil
}

const (
	redisAddr        = "redis.addr"
	redisNumReplicas = "redis.numreplicas"
)

func init() {
	ycsb.RegisterDBCreator("redis", redisCreator{})
}
