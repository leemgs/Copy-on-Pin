package etcd

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"strings"
	"time"

	"github.com/magiconair/properties"
	"github.com/pingcap/go-ycsb/pkg/ycsb"
	clientv3 "go.etcd.io/etcd/client/v3"
)

func assert(cond bool) {
	if !cond {
		panic("sanity assertion failed!")
	}
}

type etcdClient struct {
	client  *clientv3.Client
	useInts bool
}

func (etcd *etcdClient) ToSqlDB() *sql.DB {
	return nil
}

func (etcd *etcdClient) Close() error {
	return etcd.client.Close()
}

func (etcd *etcdClient) InitThread(ctx context.Context, _ int, _ int) context.Context {
	return ctx
}

func (etcd *etcdClient) CleanupThread(_ context.Context) {
}

func (etcd *etcdClient) readCount(ctx context.Context, table string, key string, count int64, fields []string) ([]map[string][]byte, error) {
	var results []map[string][]byte
	var shouldHave map[string]bool

	currentKeyStr := table + "/" + key

	for count > 0 {
		resp, err := etcd.client.Get(ctx, currentKeyStr, clientv3.WithFromKey(), clientv3.WithLimit(count))
		if err != nil {
			return nil, err
		}
		count -= int64(len(resp.Kvs))

		for _, kv := range resp.Kvs {
			// make currentKeyStr follow the "biggest" key the server sent back
			kStr := string(kv.Key)
			if kStr > currentKeyStr {
				currentKeyStr = kStr
			}

			if etcd.useInts {
				// don't bother parsing, it's just ints
				results = append(results, make(map[string][]byte))
				continue
			}
			var result map[string][]byte
			err = json.Unmarshal(kv.Value, &result)
			if err != nil {
				return nil, err
			}
			// ensure we only return the fields we need
			if len(fields) != 0 {
				if shouldHave == nil {
					shouldHave = make(map[string]bool)
					for _, field := range fields {
						shouldHave[field] = true
					}
				}
				for field := range result {
					if !shouldHave[field] {
						delete(result, field)
					}
				}
			}
			results = append(results, result)
		}
		if !resp.More {
			break
		}
	}
	return results, nil
}

func (etcd *etcdClient) Read(ctx context.Context, table string, key string, fields []string) (map[string][]byte, error) {
	results, err := etcd.readCount(ctx, table, key, 1, fields)
	if err != nil {
		return nil, err
	}
	assert(len(results) == 1)
	return results[0], nil
}

func (etcd *etcdClient) Scan(ctx context.Context, table string, startKey string, count int, fields []string) ([]map[string][]byte, error) {
	return etcd.readCount(ctx, table, startKey, int64(count), fields)
}

func (etcd *etcdClient) Update(ctx context.Context, table string, key string, values map[string][]byte) error {
	result, err := etcd.Read(ctx, table, key, nil)
	if err != nil {
		return err
	}
	for k := range values {
		result[k] = values[k]
	}
	return etcd.Insert(ctx, table, key, result)
}

func (etcd *etcdClient) Insert(ctx context.Context, table string, key string, values map[string][]byte) error {
	valuesBytes, err := json.Marshal(values)
	if err != nil {
		return err
	}
	var content string
	if etcd.useInts {
		content = fmt.Sprintf("%d", len(valuesBytes))
	} else {
		content = string(valuesBytes)
	}
	// don't think there's anything useful we can do with response in this case
	_, err = etcd.client.Put(ctx, table+"/"+key, content)
	return err
}

func (etcd *etcdClient) Delete(ctx context.Context, table string, key string) error {
	_, err := etcd.client.Delete(ctx, table+"/"+key)
	return err
}

type etcdCreator struct{}

const (
	etcdEndpoints   = "etcd.endpoints"
	etcdDialTimeout = "etcd.dialtimeout"
	etcdUseInts     = "ycsb.useints"
)

func (crt etcdCreator) Create(prop *properties.Properties) (ycsb.DB, error) {
	endpointsStr, ok := prop.Get(etcdEndpoints)
	if !ok {
		return nil, fmt.Errorf("%s must be specified in properties", etcdEndpoints)
	}
	endpoints := strings.Split(endpointsStr, ",")

	client, err := clientv3.New(clientv3.Config{
		Endpoints:   endpoints,
		DialTimeout: prop.GetParsedDuration(etcdDialTimeout, time.Second*5),
	})
	if err != nil {
		return nil, err
	}
	return &etcdClient{
		client:  client,
		useInts: prop.GetBool(etcdUseInts, false),
	}, nil
}

func init() {
	ycsb.RegisterDBCreator("etcd", etcdCreator{})
}
