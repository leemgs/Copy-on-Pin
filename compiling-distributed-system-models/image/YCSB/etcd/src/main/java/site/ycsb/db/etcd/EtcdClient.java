package site.ycsb.db.etcd;

import com.google.protobuf.ByteString;
import com.google.protobuf.CodedInputStream;
import com.google.protobuf.CodedOutputStream;
import io.etcd.jetcd.ByteSequence;
import io.etcd.jetcd.Client;
import io.etcd.jetcd.KV;
import io.etcd.jetcd.KeyValue;
import io.etcd.jetcd.kv.DeleteResponse;
import io.etcd.jetcd.kv.GetResponse;
import io.etcd.jetcd.kv.PutResponse;
import site.ycsb.ByteIterator;
import site.ycsb.DB;
import site.ycsb.DBException;
import site.ycsb.Status;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.ExecutionException;

/**
 * Client that calls through to the etcd-java library.
 */
public class EtcdClient extends DB {
  private Client client;
  private KV kv;

  private static final String ETCD_ENDPOINTS = "etcd.endpoints";

  @Override
  public void init() throws DBException {
    Properties props = getProperties();

    client = Client.builder()
        .endpoints(props.getProperty(ETCD_ENDPOINTS).split(","))
        .build();
    kv = client.getKVClient();
  }

  @Override
  public void cleanup() throws DBException {
    if(client != null) {
      client.close();
      client = null;
    }
  }

  private void parseValues(ByteSequence byteSeq, Set<String> fields,
                           Map<String, ByteIterator> result) throws IOException {
    CodedInputStream in = CodedInputStream.newInstance(byteSeq.getBytes());
    while(!in.isAtEnd()) {
      String key = in.readString();
      if(fields == null || fields.contains(key)) {
        result.put(key, new ByteStringByteIterator(in.readBytes()));
      }
    }
  }

  private ByteSequence serializeValues(Map<String, ByteIterator> values) throws IOException {
    try(ByteString.Output bufOut = ByteString.newOutput()) {
      CodedOutputStream out = CodedOutputStream.newInstance(bufOut);
      int fieldNum = 0;
      for (Map.Entry<String, ByteIterator> entry : values.entrySet()) {
        out.writeString(fieldNum, entry.getKey());
        fieldNum += 1;
        try (ByteString.Output bytesOut = ByteString.newOutput((int) entry.getValue().bytesLeft())) {
          ByteIterator iter = entry.getValue();
          while (iter.hasNext()) {
            bytesOut.write(iter.nextByte());
          }
          out.writeBytes(fieldNum, bytesOut.toByteString());
        }
      }
      return ByteSequence.from(bufOut.toByteString());
    }
  }

  private ByteSequence mkKey(String table, String key) {
    return ByteSequence.from(table, StandardCharsets.UTF_8)
        .concat(ByteSequence.from(".", StandardCharsets.UTF_8))
        .concat(ByteSequence.from(key, StandardCharsets.UTF_8));
  }

  @Override
  public Status read(String table, String key, Set<String> fields, Map<String, ByteIterator> result) {
    try {
      GetResponse response = kv
          .get(mkKey(table, key))
          .get();
      List<KeyValue> kvs = response.getKvs();
      if(kvs.size() == 0) {
        return Status.NOT_FOUND;
      } else if(kvs.size() > 1) {
        return Status.UNEXPECTED_STATE;
      }
      parseValues(kvs.get(0).getValue(), fields, result);
      return Status.OK;
    } catch (InterruptedException | ExecutionException | IOException e) {
      e.printStackTrace();
      return Status.ERROR;
    }
  }

  @Override
  public Status scan(String table, String startkey, int recordcount, Set<String> fields,
                     Vector<HashMap<String, ByteIterator>> result) {
    return Status.NOT_IMPLEMENTED;
  }

  @Override
  public Status update(String table, String key, Map<String, ByteIterator> values) {
    Map<String, ByteIterator> origValues = new HashMap<>();
    Status readStatus = read(table, key, null, origValues);
    if(readStatus != Status.OK) {
      return readStatus;
    }
    origValues.putAll(values);
    return insert(table, key, origValues);
  }

  @Override
  public Status insert(String table, String key, Map<String, ByteIterator> values) {
    try {
      PutResponse response = kv
          .put(mkKey(table, key), serializeValues(values))
          .get();
      return Status.OK;
    } catch (IOException | ExecutionException | InterruptedException e) {
      e.printStackTrace();
      return Status.ERROR;
    }
  }

  @Override
  public Status delete(String table, String key) {
    try {
      DeleteResponse response = kv
          .delete(mkKey(table, key))
          .get();
      if(response.getDeleted() == 0) {
        return Status.NOT_FOUND;
      } else if(response.getDeleted() > 1) {
        return Status.UNEXPECTED_STATE;
      }
      return Status.OK;
    } catch (ExecutionException | InterruptedException e) {
      e.printStackTrace();
      return Status.ERROR;
    }
  }

  private static class ByteStringByteIterator extends ByteIterator {
    private final ByteString.ByteIterator iter;
    private long off = 0;
    private final long len;

    public ByteStringByteIterator(ByteString byteString) {
      iter = byteString.iterator();
      len = byteString.size();
    }

    @Override
    public boolean hasNext() {
      return iter.hasNext();
    }

    @Override
    public byte nextByte() {
      byte result = iter.nextByte();
      off++;
      return result;
    }

    @Override
    public long bytesLeft() {
      return len - off;
    }
  }
}
