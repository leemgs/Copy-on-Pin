package site.ycsb.db.vard;

import scala.runtime.BoxedUnit;
import scala.util.Try;
import site.ycsb.*;

import java.io.*;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.*;

/**
 * Client for the Vard key-value store.
 */
public class VardClient extends DB {
  private com.github.fhackett.vardclient.VardClient vardClient;

  private static final String VARD_ENDPOINTS = "vard.endpoints";
  private static final String VARD_IVY_MODE = "vard.ivy_mode";

  @Override
  public void init() throws DBException {
    Properties properties = getProperties();

    vardClient = com.github.fhackett.vardclient.VardClient.builder()
        .withEndpoints(properties.getProperty(VARD_ENDPOINTS))
        .withIvyMode(Boolean.parseBoolean(properties.getProperty(VARD_IVY_MODE, "false")))
        .build();
  }

  @Override
  public void cleanup() throws DBException {
    if(vardClient != null) {
      vardClient.close();
    }
  }

  private void deserializeValues(ByteBuffer buffer, Map<String, ByteIterator> result) {
//    try {
//      // do nothing
////      ByteArrayInputStream byteArrayInputStream =
////          new ByteArrayInputStream(buffer.array(), buffer.position(), buffer.limit());
////      DataInputStream in = new DataInputStream(byteArrayInputStream);
////      while(byteArrayInputStream.available() > 0) {
////        String name = in.readUTF();
////        int len = in.readShort();
//////        ByteArrayOutputStream builder = new ByteArrayOutputStream();
//////        for(int i = 0; i < len; i++) {
//////          builder.write(in.readByte());
//////        }
//////        result.put(name, new ByteArrayByteIterator(builder.toByteArray()));
////        result.put(name, new ByteArrayByteIterator(new byte[len]));
////      }
//    } catch (IOException e) {
//      throw new RuntimeException(e);
//    }
  }

  @Override
  public Status read(String table, String key, Set<String> fields, Map<String, ByteIterator> result) {
    Try<ByteBuffer> resultBuffer = vardClient.get(StandardCharsets.UTF_8.encode(table + "/" + key));
    if(resultBuffer.isFailure()) {
      if(resultBuffer.failed().get() instanceof com.github.fhackett.vardclient.VardClient.NotFoundError) {
        return Status.OK;
      }
      resultBuffer.failed().get().printStackTrace();
      return Status.ERROR;
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

  private ByteBuffer serializeValues(Map<String, ByteIterator> values) {
    try {
      ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
      DataOutputStream out = new DataOutputStream(byteArrayOutputStream);
      out.writeShort(values.size());
//      for(Map.Entry<String, ByteIterator> entry : values.entrySet()) {
//        out.writeUTF(entry.getKey());
//        byte[] array = entry.getValue().toArray();
//        out.writeShort(array.length);
//        //out.write(array);
//      }
      out.flush();
      return ByteBuffer.wrap(byteArrayOutputStream.toByteArray());
    } catch (IOException e) {
      throw new RuntimeException(e);
    }
  }

  @Override
  public Status insert(String table, String key, Map<String, ByteIterator> values) {
    Try<BoxedUnit> result = vardClient.put(
        StandardCharsets.UTF_8.encode(table + "/" + key), serializeValues(values));
    if(result.isFailure()) {
      result.failed().get().printStackTrace();
      return Status.ERROR;
    } else {
      return Status.OK;
    }
  }

  @Override
  public Status delete(String table, String key) {
    Try<BoxedUnit> result = vardClient.del(StandardCharsets.UTF_8.encode(table + "/" + key));
    if(result.isFailure()) {
      result.failed().get().printStackTrace();
      return Status.ERROR;
    } else {
      return Status.OK;
    }
  }
}
