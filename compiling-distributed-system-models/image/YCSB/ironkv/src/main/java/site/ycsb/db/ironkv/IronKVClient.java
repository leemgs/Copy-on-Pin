package site.ycsb.db.ironkv;

import scala.runtime.BoxedUnit;
import scala.util.Try;
import site.ycsb.*;

import java.io.*;
import java.util.*;

/**
 * Client for the Vard key-value store.
 */
public class IronKVClient extends DB {
  private com.github.fhackett.ironkvclient.IronKVClient kvClient;

  private static final String IRONKV_CONFIG_FILE = "ironkv.config_file";
  private static final String IRONKV_TIMEOUT = "ironkv.timeout";
  private static final String IRONKV_DEBUG = "ironkv.debug";

  @Override
  public void init() throws DBException {
    Properties properties = getProperties();

    kvClient = com.github.fhackett.ironkvclient.IronKVClientConfig
        .fromFile(properties.getProperty(IRONKV_CONFIG_FILE))
        .withTimeout(Integer.parseInt(properties.getProperty(IRONKV_TIMEOUT, "1000")))
        .withDebug(Boolean.parseBoolean(properties.getProperty(IRONKV_DEBUG, "false")))
        .build();
  }

  @Override
  public void cleanup() throws DBException {
    if(kvClient != null) {
      kvClient.close();
    }
  }

  private void deserializeValues(byte[] buffer, Map<String, ByteIterator> result) {
    try {
      ByteArrayInputStream byteArrayInputStream = new ByteArrayInputStream(buffer);
      DataInputStream in = new DataInputStream(byteArrayInputStream);
      while(byteArrayInputStream.available() > 0) {
        String name = in.readUTF();
        int len = in.readShort();
        ByteArrayOutputStream builder = new ByteArrayOutputStream();
        for(int i = 0; i < len; i++) {
          builder.write(in.readByte());
        }
        result.put(name, new ByteArrayByteIterator(builder.toByteArray()));
      }
    } catch (IOException e) {
      throw new RuntimeException(e);
    }
  }

  @Override
  public Status read(String table, String key, Set<String> fields, Map<String, ByteIterator> result) {
    Try<byte[]> resultBuffer = kvClient.get((table + "/" + key).getBytes());
    if(resultBuffer.isFailure()) {
      if(resultBuffer.failed().get() instanceof com.github.fhackett.ironkvclient.IronKVClient.GetFailedError) {
        return Status.ERROR;
      } else {
        throw new RuntimeException(resultBuffer.failed().get());
      }
    }
    deserializeValues(resultBuffer.get(), result);
    return Status.OK;
  }

  @Override
  public Status scan(String table, String startkey, int recordcount, Set<String> fields,
                     Vector<HashMap<String, ByteIterator>> result) {
    return Status.NOT_IMPLEMENTED;
  }

  @Override
  public Status update(String table, String key, Map<String, ByteIterator> values) {
    Map<String, ByteIterator> result = new HashMap<>();
    Status status = read(table, key, null, result);
    if(status != Status.OK) {
      return status;
    }
    result.putAll(values);
    return insert(table, key, result);
  }

  private byte[] serializeValues(Map<String, ByteIterator> values) {
    try {
      ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
      DataOutputStream out = new DataOutputStream(byteArrayOutputStream);
      for(Map.Entry<String, ByteIterator> entry : values.entrySet()) {
        out.writeUTF(entry.getKey());
        byte[] array = entry.getValue().toArray();
        out.writeShort(array.length);
        out.write(array);
      }
      out.flush();
      return byteArrayOutputStream.toByteArray();
    } catch (IOException e) {
      throw new RuntimeException(e);
    }
  }

  @Override
  public Status insert(String table, String key, Map<String, ByteIterator> values) {
    Try<BoxedUnit> result = kvClient.put((table + "/" + key).getBytes(), serializeValues(values));
    if(result.isFailure()) {
      if(result.failed().get() instanceof com.github.fhackett.ironkvclient.IronKVClient.PutFailedError) {
        return Status.ERROR;
      } else {
        throw new RuntimeException(result.failed().get());
      }
    } else {
      return Status.OK;
    }
  }

  @Override
  public Status delete(String table, String key) {
    Try<BoxedUnit> result = kvClient.del((table + "/" + key).getBytes());
    if(result.isFailure()) {
      if(result.failed().get() instanceof com.github.fhackett.ironkvclient.IronKVClient.DeleteFailedError) {
        return Status.ERROR;
      } else {
        throw new RuntimeException(result.failed().get());
      }
    } else {
      return Status.OK;
    }
  }
}
