package vard

import (
	"bytes"
	"context"
	"database/sql"
	"encoding/base32"
	"encoding/binary"
	"encoding/gob"
	"fmt"
	"github.com/google/uuid"
	"github.com/magiconair/properties"
	"github.com/pingcap/go-ycsb/pkg/ycsb"
	"io"
	"log"
	"net"
	"regexp"
	"strings"
	"time"
)

type vardClientTag struct{}

type vardClient struct {
	clientId    uuid.UUID
	endpointIdx int
	conn        net.Conn
	requestId   int
	buffer      bytes.Buffer
}

type vardConfig struct {
	dialTimeout time.Duration
	endpoints   []string
}

var responseRx = regexp.MustCompile("Response\\W+([0-9]+)\\W+([/A-Za-z0-9]+|-)\\W+([/A-Za-z0-9]+|-)\\W+([/A-Za-z0-9]+|-)")

var safeEncoding = base32.StdEncoding.WithPadding('/')

func (conf *vardConfig) ToSqlDB() *sql.DB {
	return nil
}

func (conf *vardConfig) Close() error {
	return nil
}

func (conf *vardConfig) InitThread(ctx context.Context, _ int, _ int) context.Context {
	client := &vardClient{
		clientId: uuid.New(),
	}
	return context.WithValue(ctx, vardClientTag{}, client)
}

func (conf *vardConfig) setupConn(ctx context.Context) {
	client := ctx.Value(vardClientTag{}).(*vardClient)
	if client.conn == nil {
		var err error
		for {
			if err != nil {
				log.Printf("client %v error establishing connection: %v", client.clientId, err)
				client.conn = nil
				err = nil
			}
			endpoint := conf.endpoints[client.endpointIdx]
			client.endpointIdx = (client.endpointIdx + 1) % len(conf.endpoints)

			client.conn, err = net.DialTimeout("tcp", endpoint, conf.dialTimeout)
			if err != nil {
				continue
			}
			// send client ID (has to be 32 chars of hex)
			clientIdStr := strings.Replace(client.clientId.String(), "-", "", 4)
			err = binary.Write(client.conn, binary.LittleEndian, int32(len(clientIdStr)))
			if err != nil {
				continue
			}
			_, err = client.conn.Write([]byte(clientIdStr))
			if err != nil {
				continue
			}
			return
		}
	}
}

func (conf *vardConfig) procMsg(ctx context.Context, cmd string, arg1, arg2, arg3 string) ([]string, error) {
	client := ctx.Value(vardClientTag{}).(*vardClient)
	var err error
	for {
		conf.setupConn(ctx)
		results := func() []string {
			defer func() {
				if err != nil {
					log.Printf("client %v error handling msg %s: %v", client.clientId, cmd, err)
					err = client.conn.Close()
					if err != nil {
						log.Printf("client %v error closing connection: %v", client.clientId, err)
						err = nil
					}
					client.conn = nil
				}
			}()

			msg := fmt.Sprintf("%d %s %s %s %s", client.requestId, cmd, arg1, arg2, arg3)
			err = binary.Write(client.conn, binary.LittleEndian, int32(len(msg)))
			if err != nil {
				return nil
			}
			_, err = client.conn.Write([]byte(msg))
			if err != nil {
				return nil
			}
			client.requestId += 1

			// wait for response...
			var responseLen int32
			err = binary.Read(client.conn, binary.LittleEndian, &responseLen)
			if err != nil {
				return nil
			}
			client.buffer.Reset()
			client.buffer.Grow(int(responseLen))
			_, err = io.CopyN(&client.buffer, client.conn, int64(responseLen))
			if err != nil {
				return nil
			}

			responseStr := client.buffer.String()
			if strings.HasPrefix(responseStr, "NotLeader") {
				err = fmt.Errorf("%s was not leader", client.conn.RemoteAddr().String())
				return nil
			}
			matches := responseRx.FindStringSubmatchIndex(responseStr)
			if matches == nil {
				err = fmt.Errorf("could not parse response `%s` from %s", responseStr, client.conn.RemoteAddr().String())
				return nil
			}

			var results []string
			for i := 0; i < len(matches); i += 2 {
				results = append(results, responseStr[matches[i]:matches[i+1]])
			}
			return results[1:] // element 0 is the entire string!
		}()
		if results != nil {
			return results, nil
		}
	}

}

func (conf *vardConfig) CleanupThread(ctx context.Context) {
	client := ctx.Value(vardClientTag{}).(*vardClient)
	if client.conn != nil {
		err := client.conn.Close()
		if err != nil {
			log.Printf("error closing client %s connection to %s: %v", client.clientId, client.conn.RemoteAddr().String(), err)
		}
	}
}

func (conf *vardConfig) Read(ctx context.Context, table string, key string, fields []string) (map[string][]byte, error) {
	_, err := conf.procMsg(ctx, "GET", table+"/"+key, "-", "-")
	if err != nil {
		return nil, err
	}
	result := make(map[string][]byte)
	//data := results[2]
	//decoder := gob.NewDecoder(base32.NewDecoder(safeEncoding, bytes.NewBufferString(data)))
	//var result map[string][]byte
	//err = decoder.Decode(&result)
	//if err != nil {
	//	panic(err)
	//}

	if fields != nil {
		includeField := make(map[string]bool)
		for _, field := range fields {
			includeField[field] = true
		}
		for k := range result {
			if !includeField[k] {
				delete(result, k)
			}
		}
	}

	return result, nil
}

func (conf *vardConfig) Scan(_ context.Context, _ string, _ string, _ int, _ []string) ([]map[string][]byte, error) {
	return nil, fmt.Errorf("vard does not support scan")
}

func (conf *vardConfig) Update(ctx context.Context, table string, key string, values map[string][]byte) error {
	result, err := conf.Read(ctx, table, key, nil)
	if err != nil {
		return err
	}
	for k := range values {
		result[k] = values[k]
	}
	return conf.Insert(ctx, table, key, values)
}

func (conf *vardConfig) Insert(ctx context.Context, table string, key string, values map[string][]byte) error {
	var packedValueBuf strings.Builder
	b32Encoder := base32.NewEncoder(safeEncoding, &packedValueBuf)
	encoder := gob.NewEncoder(b32Encoder)
	err := encoder.Encode(&values)
	if err != nil {
		panic(err)
	}
	_ = b32Encoder.Close() // add padding to end

	_, err = conf.procMsg(ctx, "PUT", table+"/"+key, fmt.Sprintf("%d", packedValueBuf.Len()), "-")
	return err
}

func (conf *vardConfig) Delete(ctx context.Context, table string, key string) error {
	_, err := conf.procMsg(ctx, "DEL", table+"/"+key, "-", "-")
	return err
}

type vardCreator struct{}

const (
	vardEndpoints   = "vard.endpoints"
	vardDialTimeout = "vard.dialtimeout"
)

func (_ vardCreator) Create(props *properties.Properties) (ycsb.DB, error) {
	endpointsStr, ok := props.Get(vardEndpoints)
	if !ok {
		return nil, fmt.Errorf("%s must be specified", vardEndpoints)
	}
	endpoints := strings.Split(endpointsStr, ",")

	return &vardConfig{
		endpoints:   endpoints,
		dialTimeout: props.GetParsedDuration(vardDialTimeout, time.Second*5),
	}, nil
}

func init() {
	ycsb.RegisterDBCreator("vard", vardCreator{})
}
