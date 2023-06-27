-------------------------------- MODULE shopcart --------------------------------

\* do not check for deadlocks.

EXTENDS Naturals, Sequences, TLC, FiniteSets

CONSTANT NUM_NODES
CONSTANT ELEM_SET
CONSTANT BENCH_NUM_ROUNDS

(********************

--mpcal shopcart {
    define {
        NODE_SET == 1..NUM_NODES

        NULL == [n \in NODE_SET |-> 0]

        ELEM1 == "1"
        ELEM2 == "2"
        ELEM3 == "3"

        AddCmd == 1
        RemoveCmd == 2

        AddStart     == 0
        AddFinish    == 1

        Max(a, b) == IF a > b THEN a ELSE b
        MergeVectorClock(v1, v2) == [i \in DOMAIN v1 |-> Max(v1[i], v2[i])]

        \* returns TRUE if v1 < v2 otherwise FALSE
        CompareVectorClock(v1, v2) == IF \A i \in DOMAIN v1: v1[i] <= v2[i] THEN TRUE ELSE FALSE

        MergeKeys(a, b) == [k \in DOMAIN a |-> MergeVectorClock(a[k], b[k])]

        QUERY(r) == {elem \in DOMAIN r.addMap: ~CompareVectorClock(r.addMap[elem], r.remMap[elem])} 

        isOKSet(xset, round) == \A i \in NODE_SET: \A j \in 0..round: <<i, j>> \in xset
    }

    macro Add(crdt, self, elem) {
        crdt[self] := [cmd |-> AddCmd, elem |-> elem];
    }

    macro Remove(crdt, self, elem) {
        crdt[self] := [cmd |-> RemoveCmd, elem |-> elem];
    }

    macro Merge(crdt, i1, i2) {
        assert crdt[i1] # crdt[i2];
        with (addk = MergeKeys(crdt[i1].addMap, crdt[i2].addMap), remk = MergeKeys(crdt[i1].remMap, crdt[i2].remMap)) {
            with (add = [i \in DOMAIN addk |-> IF CompareVectorClock(addk[i], remk[i]) THEN NULL ELSE addk[i]]) {
                crdt[i1].addMap := add;
                crdt[i2].addMap := add;
                assert crdt[i1].addMap = crdt[i2].addMap;
            };
            with (rem = [i \in DOMAIN remk |-> IF CompareVectorClock(addk[i], remk[i]) THEN remk[i] ELSE NULL]) {
                crdt[i1].remMap := rem;
                crdt[i2].remMap := rem;
                assert crdt[i1].remMap = crdt[i2].remMap;
            };
        };
        assert crdt[i1] = crdt[i2];
    }

    mapping macro AWORSet {
        read {
            yield QUERY($variable);
        }

        write {
            if ($value.cmd = AddCmd) { 
                if ($variable.addMap[$value.elem] # NULL) {
                    $variable.addMap[$value.elem][self] := $variable.addMap[$value.elem][self] + 1;
                    $variable.remMap[$value.elem] := NULL;
                } else if ($variable.remMap[$value.elem] # NULL) {
                    $variable.addMap[$value.elem][self] := $variable.remMap[$value.elem][self] + 1;
                    $variable.remMap[$value.elem] := NULL;
                } else {
                    $variable.addMap[$value.elem][self] := 1;
                };
            } else if ($value.cmd = RemoveCmd) {
                if ($variable.remMap[$value.elem] # NULL) {
                    $variable.remMap[$value.elem][self] := $variable.remMap[$value.elem][self] + 1;
                    $variable.addMap[$value.elem] := NULL;
                } else if ($variable.addMap[$value.elem] # NULL) {
                    $variable.remMap[$value.elem][self] := $variable.addMap[$value.elem][self] + 1;
                    $variable.addMap[$value.elem] := NULL;
                } else {
                    $variable.remMap[$value.elem][self] := 1;
                };
            };
        }
    }

    mapping macro InputQueue {
        read {
            await Len($variable) > 0;
            with (r = Head($variable)) {
                $variable := Tail($variable);
                yield r;
            };
        }

        write {
            yield Append($variable, $value);
        }
    }

    archetype ANode(ref crdt[_], ref in, ref out) {
    nodeLoop:
        while (TRUE) {
            with (req = in) {
                if (req.cmd = AddCmd) {
                    Add(crdt, self, req.elem);
                } else if (req.cmd = RemoveCmd) {
                    Remove(crdt, self, req.elem);
                };
            };
        
        rcvResp:
            out := crdt[self];
        };
    }

    archetype ANodeBench(ref crdt[_], ref out, ref c[_])
    variable r = 0;
    {
    nodeBenchLoop:
        while (r < BENCH_NUM_ROUNDS) {
        add:
            Add(crdt, self, <<self, r>>);
            c[self] := c[self] \cup {<<AddCmd, self, r>>};
            out := [node |-> self, event |-> AddStart];
        waitAdd:
            await isOKSet(crdt[self], r);
            out := [node |-> self, event |-> AddFinish];
            r := r + 1;
        };
    }

    variable
        crdt = [nid \in NODE_SET |-> [addMap |-> [eid \in ELEM_SET |-> NULL], remMap |-> [eid \in ELEM_SET |-> NULL]]];
        in = <<
            [cmd |-> AddCmd, elem |-> ELEM1],
            [cmd |-> RemoveCmd, elem |-> ELEM2],
            [cmd |-> AddCmd, elem |-> ELEM2],
            [cmd |-> RemoveCmd, elem |-> ELEM1]
        >>;
        out;
        c = [id \in NODE_SET |-> {}];

    \* fair process (Node \in NODE_SET) == instance ANode(ref crdt[_], ref in, ref out)
    \*     mapping crdt[_] via AWORSet
    \*     mapping in via InputQueue;

    fair process (Node \in NODE_SET) == instance ANodeBench(ref crdt[_], ref out, ref c[_])
        mapping crdt[_] via AWORSet;

    fair process (UpdateCRDT = 0) {
    l1:
        while (TRUE) {
            with (i1 \in NODE_SET; i2 \in {x \in NODE_SET: crdt[x] # crdt[i1]}) {
                Merge(crdt, i1, i2);
                with (cn = c[i1] \cup c[i2]) {
                    c[i1] := cn;
                    c[i2] := cn;
                };
            };
        };
    }
}

\* BEGIN PLUSCAL TRANSLATION
--algorithm shopcart {
  variables crdt = [nid \in NODE_SET |-> [addMap |-> [eid \in ELEM_SET |-> NULL], remMap |-> [eid \in ELEM_SET |-> NULL]]]; in = <<[cmd |-> AddCmd, elem |-> ELEM1], [cmd |-> RemoveCmd, elem |-> ELEM2], [cmd |-> AddCmd, elem |-> ELEM2], [cmd |-> RemoveCmd, elem |-> ELEM1]>>; out; c = [id \in NODE_SET |-> {}];
  define{
    NODE_SET == (1) .. (NUM_NODES)
    NULL == [n \in NODE_SET |-> 0]
    ELEM1 == "1"
    ELEM2 == "2"
    ELEM3 == "3"
    AddCmd == 1
    RemoveCmd == 2
    AddStart == 0
    AddFinish == 1
    Max(a, b) == IF (a) > (b) THEN a ELSE b
    MergeVectorClock(v1, v2) == [i \in DOMAIN (v1) |-> Max((v1)[i], (v2)[i])]
    CompareVectorClock(v1, v2) == IF \A i \in DOMAIN (v1) : ((v1)[i]) <= ((v2)[i]) THEN TRUE ELSE FALSE
    MergeKeys(a, b) == [k \in DOMAIN (a) |-> MergeVectorClock((a)[k], (b)[k])]
    QUERY(r) == {elem \in DOMAIN ((r).addMap) : ~ (CompareVectorClock(((r).addMap)[elem], ((r).remMap)[elem]))}
    isOKSet(xset, round) == \A i \in NODE_SET : \A j \in (0) .. (round) : (<<i, j>>) \in (xset)
  }
  
  fair process (UpdateCRDT = 0)
  {
    l1:
      if (TRUE) {
        with (
          i1 \in NODE_SET, 
          i2 \in {x \in NODE_SET : ((crdt)[x]) # ((crdt)[i1])}
        ) {
          assert ((crdt)[i1]) # ((crdt)[i2]);
          with (
            addk0 = MergeKeys(((crdt)[i1]).addMap, ((crdt)[i2]).addMap), 
            remk0 = MergeKeys(((crdt)[i1]).remMap, ((crdt)[i2]).remMap), 
            add0 = [i \in DOMAIN (addk0) |-> IF CompareVectorClock((addk0)[i], (remk0)[i]) THEN NULL ELSE (addk0)[i]], 
            crdt0 = [crdt EXCEPT ![i1]["addMap"] = add0], 
            crdt1 = [crdt0 EXCEPT ![i2]["addMap"] = add0]
          ) {
            assert (((crdt1)[i1]).addMap) = (((crdt1)[i2]).addMap);
            with (
              rem0 = [i \in DOMAIN (remk0) |-> IF CompareVectorClock((addk0)[i], (remk0)[i]) THEN (remk0)[i] ELSE NULL], 
              crdt2 = [crdt1 EXCEPT ![i1]["remMap"] = rem0]
            ) {
              crdt := [crdt2 EXCEPT ![i2]["remMap"] = rem0];
              assert (((crdt)[i1]).remMap) = (((crdt)[i2]).remMap);
              assert ((crdt)[i1]) = ((crdt)[i2]);
              with (
                cn = ((c)[i1]) \union ((c)[i2]), 
                c0 = [c EXCEPT ![i1] = cn]
              ) {
                c := [c0 EXCEPT ![i2] = cn];
                goto l1;
              };
            };
          };
        };
      } else {
        goto Done;
      };
  }
  
  fair process (Node \in NODE_SET)
    variables r = 0;
  {
    nodeBenchLoop:
      if ((r) < (BENCH_NUM_ROUNDS)) {
        goto add;
      } else {
        goto Done;
      };
    add:
      with (value0 = [cmd |-> AddCmd, elem |-> <<self, r>>]) {
        if (((value0).cmd) = (AddCmd)) {
          if (((((crdt)[self]).addMap)[(value0).elem]) # (NULL)) {
            with (crdt3 = [crdt EXCEPT ![self]["addMap"][(value0).elem][self] = (((((crdt)[self]).addMap)[(value0).elem])[self]) + (1)]) {
              crdt := [crdt3 EXCEPT ![self]["remMap"][(value0).elem] = NULL];
              c := [c EXCEPT ![self] = ((c)[self]) \union ({<<AddCmd, self, r>>})];
              out := [node |-> self, event |-> AddStart];
              goto waitAdd;
            };
          } else {
            if (((((crdt)[self]).remMap)[(value0).elem]) # (NULL)) {
              with (crdt4 = [crdt EXCEPT ![self]["addMap"][(value0).elem][self] = (((((crdt)[self]).remMap)[(value0).elem])[self]) + (1)]) {
                crdt := [crdt4 EXCEPT ![self]["remMap"][(value0).elem] = NULL];
                c := [c EXCEPT ![self] = ((c)[self]) \union ({<<AddCmd, self, r>>})];
                out := [node |-> self, event |-> AddStart];
                goto waitAdd;
              };
            } else {
              crdt := [crdt EXCEPT ![self]["addMap"][(value0).elem][self] = 1];
              c := [c EXCEPT ![self] = ((c)[self]) \union ({<<AddCmd, self, r>>})];
              out := [node |-> self, event |-> AddStart];
              goto waitAdd;
            };
          };
        } else {
          if (((value0).cmd) = (RemoveCmd)) {
            if (((((crdt)[self]).remMap)[(value0).elem]) # (NULL)) {
              with (crdt5 = [crdt EXCEPT ![self]["remMap"][(value0).elem][self] = (((((crdt)[self]).remMap)[(value0).elem])[self]) + (1)]) {
                crdt := [crdt5 EXCEPT ![self]["addMap"][(value0).elem] = NULL];
                c := [c EXCEPT ![self] = ((c)[self]) \union ({<<AddCmd, self, r>>})];
                out := [node |-> self, event |-> AddStart];
                goto waitAdd;
              };
            } else {
              if (((((crdt)[self]).addMap)[(value0).elem]) # (NULL)) {
                with (crdt6 = [crdt EXCEPT ![self]["remMap"][(value0).elem][self] = (((((crdt)[self]).addMap)[(value0).elem])[self]) + (1)]) {
                  crdt := [crdt6 EXCEPT ![self]["addMap"][(value0).elem] = NULL];
                  c := [c EXCEPT ![self] = ((c)[self]) \union ({<<AddCmd, self, r>>})];
                  out := [node |-> self, event |-> AddStart];
                  goto waitAdd;
                };
              } else {
                crdt := [crdt EXCEPT ![self]["remMap"][(value0).elem][self] = 1];
                c := [c EXCEPT ![self] = ((c)[self]) \union ({<<AddCmd, self, r>>})];
                out := [node |-> self, event |-> AddStart];
                goto waitAdd;
              };
            };
          } else {
            c := [c EXCEPT ![self] = ((c)[self]) \union ({<<AddCmd, self, r>>})];
            out := [node |-> self, event |-> AddStart];
            goto waitAdd;
          };
        };
      };
    waitAdd:
      with (yielded_crdt0 = QUERY((crdt)[self])) {
        await isOKSet(yielded_crdt0, r);
        out := [node |-> self, event |-> AddFinish];
        r := (r) + (1);
        goto nodeBenchLoop;
      };
  }
}

\* END PLUSCAL TRANSLATION

********************)
\* BEGIN TRANSLATION (chksum(pcal) = "156457b4" /\ chksum(tla) = "ce5b3308")
CONSTANT defaultInitValue
VARIABLES crdt, in, out, c, pc

(* define statement *)
NODE_SET == (1) .. (NUM_NODES)
NULL == [n \in NODE_SET |-> 0]
ELEM1 == "1"
ELEM2 == "2"
ELEM3 == "3"
AddCmd == 1
RemoveCmd == 2
AddStart == 0
AddFinish == 1
Max(a, b) == IF (a) > (b) THEN a ELSE b
MergeVectorClock(v1, v2) == [i \in DOMAIN (v1) |-> Max((v1)[i], (v2)[i])]
CompareVectorClock(v1, v2) == IF \A i \in DOMAIN (v1) : ((v1)[i]) <= ((v2)[i]) THEN TRUE ELSE FALSE
MergeKeys(a, b) == [k \in DOMAIN (a) |-> MergeVectorClock((a)[k], (b)[k])]
QUERY(r) == {elem \in DOMAIN ((r).addMap) : ~ (CompareVectorClock(((r).addMap)[elem], ((r).remMap)[elem]))}
isOKSet(xset, round) == \A i \in NODE_SET : \A j \in (0) .. (round) : (<<i, j>>) \in (xset)

VARIABLE r

vars == << crdt, in, out, c, pc, r >>

ProcSet == {0} \cup (NODE_SET)

Init == (* Global variables *)
        /\ crdt = [nid \in NODE_SET |-> [addMap |-> [eid \in ELEM_SET |-> NULL], remMap |-> [eid \in ELEM_SET |-> NULL]]]
        /\ in = <<[cmd |-> AddCmd, elem |-> ELEM1], [cmd |-> RemoveCmd, elem |-> ELEM2], [cmd |-> AddCmd, elem |-> ELEM2], [cmd |-> RemoveCmd, elem |-> ELEM1]>>
        /\ out = defaultInitValue
        /\ c = [id \in NODE_SET |-> {}]
        (* Process Node *)
        /\ r = [self \in NODE_SET |-> 0]
        /\ pc = [self \in ProcSet |-> CASE self = 0 -> "l1"
                                        [] self \in NODE_SET -> "nodeBenchLoop"]

l1 == /\ pc[0] = "l1"
      /\ IF TRUE
            THEN /\ \E i1 \in NODE_SET:
                      \E i2 \in {x \in NODE_SET : ((crdt)[x]) # ((crdt)[i1])}:
                        /\ Assert(((crdt)[i1]) # ((crdt)[i2]), 
                                  "Failure of assertion at line 204, column 11.")
                        /\ LET addk0 == MergeKeys(((crdt)[i1]).addMap, ((crdt)[i2]).addMap) IN
                             LET remk0 == MergeKeys(((crdt)[i1]).remMap, ((crdt)[i2]).remMap) IN
                               LET add0 == [i \in DOMAIN (addk0) |-> IF CompareVectorClock((addk0)[i], (remk0)[i]) THEN NULL ELSE (addk0)[i]] IN
                                 LET crdt0 == [crdt EXCEPT ![i1]["addMap"] = add0] IN
                                   LET crdt1 == [crdt0 EXCEPT ![i2]["addMap"] = add0] IN
                                     /\ Assert((((crdt1)[i1]).addMap) = (((crdt1)[i2]).addMap), 
                                               "Failure of assertion at line 212, column 13.")
                                     /\ LET rem0 == [i \in DOMAIN (remk0) |-> IF CompareVectorClock((addk0)[i], (remk0)[i]) THEN (remk0)[i] ELSE NULL] IN
                                          LET crdt2 == [crdt1 EXCEPT ![i1]["remMap"] = rem0] IN
                                            /\ crdt' = [crdt2 EXCEPT ![i2]["remMap"] = rem0]
                                            /\ Assert((((crdt')[i1]).remMap) = (((crdt')[i2]).remMap), 
                                                      "Failure of assertion at line 218, column 15.")
                                            /\ Assert(((crdt')[i1]) = ((crdt')[i2]), 
                                                      "Failure of assertion at line 219, column 15.")
                                            /\ LET cn == ((c)[i1]) \union ((c)[i2]) IN
                                                 LET c0 == [c EXCEPT ![i1] = cn] IN
                                                   /\ c' = [c0 EXCEPT ![i2] = cn]
                                                   /\ pc' = [pc EXCEPT ![0] = "l1"]
            ELSE /\ pc' = [pc EXCEPT ![0] = "Done"]
                 /\ UNCHANGED << crdt, c >>
      /\ UNCHANGED << in, out, r >>

UpdateCRDT == l1

nodeBenchLoop(self) == /\ pc[self] = "nodeBenchLoop"
                       /\ IF (r[self]) < (BENCH_NUM_ROUNDS)
                             THEN /\ pc' = [pc EXCEPT ![self] = "add"]
                             ELSE /\ pc' = [pc EXCEPT ![self] = "Done"]
                       /\ UNCHANGED << crdt, in, out, c, r >>

add(self) == /\ pc[self] = "add"
             /\ LET value0 == [cmd |-> AddCmd, elem |-> <<self, r[self]>>] IN
                  IF ((value0).cmd) = (AddCmd)
                     THEN /\ IF ((((crdt)[self]).addMap)[(value0).elem]) # (NULL)
                                THEN /\ LET crdt3 == [crdt EXCEPT ![self]["addMap"][(value0).elem][self] = (((((crdt)[self]).addMap)[(value0).elem])[self]) + (1)] IN
                                          /\ crdt' = [crdt3 EXCEPT ![self]["remMap"][(value0).elem] = NULL]
                                          /\ c' = [c EXCEPT ![self] = ((c)[self]) \union ({<<AddCmd, self, r[self]>>})]
                                          /\ out' = [node |-> self, event |-> AddStart]
                                          /\ pc' = [pc EXCEPT ![self] = "waitAdd"]
                                ELSE /\ IF ((((crdt)[self]).remMap)[(value0).elem]) # (NULL)
                                           THEN /\ LET crdt4 == [crdt EXCEPT ![self]["addMap"][(value0).elem][self] = (((((crdt)[self]).remMap)[(value0).elem])[self]) + (1)] IN
                                                     /\ crdt' = [crdt4 EXCEPT ![self]["remMap"][(value0).elem] = NULL]
                                                     /\ c' = [c EXCEPT ![self] = ((c)[self]) \union ({<<AddCmd, self, r[self]>>})]
                                                     /\ out' = [node |-> self, event |-> AddStart]
                                                     /\ pc' = [pc EXCEPT ![self] = "waitAdd"]
                                           ELSE /\ crdt' = [crdt EXCEPT ![self]["addMap"][(value0).elem][self] = 1]
                                                /\ c' = [c EXCEPT ![self] = ((c)[self]) \union ({<<AddCmd, self, r[self]>>})]
                                                /\ out' = [node |-> self, event |-> AddStart]
                                                /\ pc' = [pc EXCEPT ![self] = "waitAdd"]
                     ELSE /\ IF ((value0).cmd) = (RemoveCmd)
                                THEN /\ IF ((((crdt)[self]).remMap)[(value0).elem]) # (NULL)
                                           THEN /\ LET crdt5 == [crdt EXCEPT ![self]["remMap"][(value0).elem][self] = (((((crdt)[self]).remMap)[(value0).elem])[self]) + (1)] IN
                                                     /\ crdt' = [crdt5 EXCEPT ![self]["addMap"][(value0).elem] = NULL]
                                                     /\ c' = [c EXCEPT ![self] = ((c)[self]) \union ({<<AddCmd, self, r[self]>>})]
                                                     /\ out' = [node |-> self, event |-> AddStart]
                                                     /\ pc' = [pc EXCEPT ![self] = "waitAdd"]
                                           ELSE /\ IF ((((crdt)[self]).addMap)[(value0).elem]) # (NULL)
                                                      THEN /\ LET crdt6 == [crdt EXCEPT ![self]["remMap"][(value0).elem][self] = (((((crdt)[self]).addMap)[(value0).elem])[self]) + (1)] IN
                                                                /\ crdt' = [crdt6 EXCEPT ![self]["addMap"][(value0).elem] = NULL]
                                                                /\ c' = [c EXCEPT ![self] = ((c)[self]) \union ({<<AddCmd, self, r[self]>>})]
                                                                /\ out' = [node |-> self, event |-> AddStart]
                                                                /\ pc' = [pc EXCEPT ![self] = "waitAdd"]
                                                      ELSE /\ crdt' = [crdt EXCEPT ![self]["remMap"][(value0).elem][self] = 1]
                                                           /\ c' = [c EXCEPT ![self] = ((c)[self]) \union ({<<AddCmd, self, r[self]>>})]
                                                           /\ out' = [node |-> self, event |-> AddStart]
                                                           /\ pc' = [pc EXCEPT ![self] = "waitAdd"]
                                ELSE /\ c' = [c EXCEPT ![self] = ((c)[self]) \union ({<<AddCmd, self, r[self]>>})]
                                     /\ out' = [node |-> self, event |-> AddStart]
                                     /\ pc' = [pc EXCEPT ![self] = "waitAdd"]
                                     /\ crdt' = crdt
             /\ UNCHANGED << in, r >>

waitAdd(self) == /\ pc[self] = "waitAdd"
                 /\ LET yielded_crdt0 == QUERY((crdt)[self]) IN
                      /\ isOKSet(yielded_crdt0, r[self])
                      /\ out' = [node |-> self, event |-> AddFinish]
                      /\ r' = [r EXCEPT ![self] = (r[self]) + (1)]
                      /\ pc' = [pc EXCEPT ![self] = "nodeBenchLoop"]
                 /\ UNCHANGED << crdt, in, c >>

Node(self) == nodeBenchLoop(self) \/ add(self) \/ waitAdd(self)

(* Allow infinite stuttering to prevent deadlock on termination. *)
Terminating == /\ \A self \in ProcSet: pc[self] = "Done"
               /\ UNCHANGED vars

Next == UpdateCRDT
           \/ (\E self \in NODE_SET: Node(self))
           \/ Terminating

Spec == /\ Init /\ [][Next]_vars
        /\ WF_vars(UpdateCRDT)
        /\ \A self \in NODE_SET : WF_vars(Node(self))

Termination == <>(\A self \in ProcSet: pc[self] = "Done")

\* END TRANSLATION 

\* Invariants

QueryOK == \A n1, n2 \in NODE_SET: ((crdt[n1] = crdt[n2]) => (QUERY(crdt[n1]) = QUERY(crdt[n2])))

\* Properties

EventualStateConvergence == []<>(\A n1, n2 \in NODE_SET: crdt[n1] = crdt[n2])
EventualValueConvergence == []<>(\A n1, n2 \in NODE_SET: QUERY(crdt[n1]) = QUERY(crdt[n2]))

EventualDelivery == <>(\A i, j \in NODE_SET: (\A f \in c[i]: f \in c[j]))

StrongConvergence == \A i, j \in NODE_SET: (c[i] = c[j]) => (crdt[i] = crdt[j])

NodeTermination == <>(\A n \in NODE_SET: pc[n] = "Done")

\* this property should be violated
\* ValueOK == <>(\A n \in NODE_SET: QUERY(crdt[n]) = {ELEM2})

=============================================================================
