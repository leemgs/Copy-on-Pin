package roshiapp

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"net/http"
	"strconv"
	"strings"
	"time"

	"github.com/shayanh/roshi/common"
)

const key = "pgo"

type Element struct {
	Node  int
	Round int
}

func parseElement(s string) (Element, error) {
	words := strings.Split(s, ",")
	if len(words) != 2 {
		return Element{}, errors.New("unexpected element format")
	}
	node, err := strconv.Atoi(words[0])
	if err != nil {
		return Element{}, err
	}
	round, err := strconv.Atoi(words[1])
	if err != nil {
		return Element{}, err
	}
	return Element{Node: node, Round: round}, err
}

func (e Element) String() string {
	return fmt.Sprintf("%d,%d", e.Node, e.Round)
}

type Client struct {
	server string
	client *http.Client
}

func NewClient(server string) *Client {
	return &Client{
		server: server,
		client: &http.Client{},
	}
}

func (c *Client) Add(e Element) error {
	items := []common.KeyScoreMember{
		{
			Key:    key,
			Score:  float64(time.Now().UnixNano()),
			Member: e.String(),
		},
	}
	reqBody, err := json.Marshal(items)
	if err != nil {
		return err
	}
	resp, err := c.client.Post(c.server, "application/json", bytes.NewBuffer(reqBody))
	if err != nil {
		return err
	}
	defer func() {
		if err := resp.Body.Close(); err != nil {
			log.Println(err)
		}
	}()
	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("non 200 response: %v", resp)
	}
	return nil
}

type ReadResponse struct {
	Duration string                             `json:"duration"`
	Records  map[string][]common.KeyScoreMember `json:"records"`
}

func (c *Client) Read() ([]Element, error) {
	keys := [][]byte{
		[]byte("pgo"),
	}
	reqBody, err := json.Marshal(keys)
	if err != nil {
		return nil, err
	}

	req, err := http.NewRequest("GET", c.server, bytes.NewBuffer(reqBody))
	if err != nil {
		return nil, err
	}
	q := req.URL.Query()
	q.Add("limit", "10000")
	req.URL.RawQuery = q.Encode()

	resp, err := c.client.Do(req)
	if err != nil {
		return nil, err
	}
	defer func() {
		if err := resp.Body.Close(); err != nil {
			log.Println(err)
		}
	}()

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("non 200 response: %s", resp.Status)
	}
	var data ReadResponse
	err = json.NewDecoder(resp.Body).Decode(&data)
	if err != nil {
		return nil, err
	}
	pgoData, ok := data.Records["pgo"]
	if !ok {
		return nil, nil
	}

	var elems []Element
	for _, ksm := range pgoData {
		element, err := parseElement(ksm.Member)
		if err != nil {
			log.Println(err)
		} else {
			elems = append(elems, element)
		}
	}
	return elems, nil
}
