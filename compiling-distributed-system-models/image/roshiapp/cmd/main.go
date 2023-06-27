package main

import (
	"flag"
	"fmt"
	"log"
	"time"

	"github.com/DistCompiler/roshiapp"
)

func isOKSet(elems []roshiapp.Element, numNodes, round int) bool {
	for i := 1; i <= numNodes; i++ {
		for j := 0; j <= round; j++ {
			found := false
			for _, elem := range elems {
				if elem.Node == i && elem.Round == j {
					found = true
				}
			}
			if !found {
				return false
			}
		}
	}
	return true
}

func main() {
	var serverIdx int
	var numNodes int
	var roshiServer string
	var numRounds int

	flag.IntVar(&serverIdx, "serverIdx", -1, "server index (0-based)")
	flag.IntVar(&numNodes, "numNodes", 0, "number of nodes")
	flag.StringVar(&roshiServer, "roshiServer", "", "roshi server address")
	flag.IntVar(&numRounds, "numRounds", 0, "number of add rounds")

	flag.Parse()

	if serverIdx == -1 || numNodes == 0 || roshiServer == "" || numRounds == 0 {
		log.Fatal("missing argument")
	}

	fmt.Println("roshi app started")

	self := serverIdx + 1

	client := roshiapp.NewClient(roshiServer)

	for i := 0; i < numRounds; i++ {
		err := client.Add(roshiapp.Element{Node: self, Round: i})
		if err != nil {
			log.Fatal(err)
		}
		start := time.Now()
		for {
			elems, err := client.Read()
			//sort.Slice(elems, func(i, j int) bool {
			//	return elems[i].Round < elems[j].Round || (elems[i].Round == elems[j].Round && elems[i].Node < elems[j].Node)
			//})
			//log.Println("node =", self, "round =", i, "elems =", elems)
			//time.Sleep(1 * time.Second)
			if err != nil {
				log.Fatal(err)
			}
			if isOKSet(elems, numNodes, i) {
				break
			}
		}
		elapsed := time.Since(start)
		fmt.Println("RESULT", i, self, elapsed)
	}
}
