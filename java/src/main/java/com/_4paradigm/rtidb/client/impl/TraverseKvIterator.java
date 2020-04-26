package com._4paradigm.rtidb.client.impl;

import com._4paradigm.rtidb.client.KvIterator;
import com._4paradigm.rtidb.client.TabletException;
import com._4paradigm.rtidb.client.ha.PartitionHandler;
import com._4paradigm.rtidb.client.ha.RTIDBClient;
import com._4paradigm.rtidb.client.ha.TableHandler;
import com._4paradigm.rtidb.client.schema.ColumnDesc;
import com._4paradigm.rtidb.client.schema.RowCodec;
import com._4paradigm.rtidb.client.schema.RowView;
import com._4paradigm.rtidb.ns.NS;
import com._4paradigm.rtidb.tablet.Tablet;
import com._4paradigm.rtidb.utils.Compress;
import com.google.protobuf.ByteString;
import rtidb.api.TabletServer;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.Charset;
import java.util.List;
import java.util.concurrent.TimeoutException;

public class TraverseKvIterator implements KvIterator {
    private int pid = 0;
    private int lastPid = 0;
    private String idxName = null;
    private String tsName = null;
    private ByteString bs;
    private int offset;
    private ByteBuffer bb;

    private ByteBuffer slice;
    private int totalSize;
    private long time;
    private long lastTime;
    private String pk;
    private String lastPk;
    private List<ColumnDesc> schema;
    private NS.CompressType compressType = NS.CompressType.kNoCompress;

    private boolean isFinished = false;

    private RTIDBClient client = null;
    private TableHandler th = null;
    private static Charset charset = Charset.forName("utf-8");
    private RowView rv;

    public TraverseKvIterator(RTIDBClient client, TableHandler th, String idxName, String tsName) {
        this.offset = 0;
        this.totalSize = 0;
        this.client = client;
        this.th = th;
        this.schema = th.getSchema();
        this.idxName = idxName;
        this.tsName = tsName;
        this.isFinished = false;
        if (th != null) {
            this.compressType = th.getTableInfo().getCompressType();
        }
        if (th.getFormatVersion() == 1) {
            rv = new RowView(th.getSchema());
        }
    }

    private void getData() throws TimeoutException, TabletException {
        do {
            if (pid >= th.getPartitions().length) {
                return;
            }
            PartitionHandler ph = th.getHandler(pid);
            TabletServer ts = ph.getReadHandler(th.getReadStrategy());
            Tablet.TraverseRequest.Builder builder = Tablet.TraverseRequest.newBuilder();
            builder.setTid(th.getTableInfo().getTid());
            builder.setPid(pid);
            builder.setLimit(client.getConfig().getTraverseLimit());
            if (idxName != null && !idxName.isEmpty()) {
                builder.setIdxName(idxName);
            }
            if (tsName != null && !tsName.isEmpty()) {
                builder.setTsName(tsName);
            }
            if (client.getConfig().isRemoveDuplicateByTime()) {
                builder.setEnableRemoveDuplicatedRecord(true);
            }
            if (offset != 0 && lastPid == pid) {
                builder.setPk(lastPk);
                builder.setTs(lastTime);
            }
            Tablet.TraverseRequest request = builder.build();
            Tablet.TraverseResponse response = ts.traverse(request);
            if (response != null && response.getCode() == 0) {
                bs = response.getPairs();
                bb = bs.asReadOnlyByteBuffer();
                totalSize = this.bs.size();
                offset = 0;
                if (totalSize == 0) {
                    if (response.hasIsFinish() && !response.getIsFinish()) {
                        lastPk = response.getPk();
                        lastTime = response.getTs();
                    } else {
                        pid++;
                    }
                    continue;
                }
                lastPid = pid;
                lastPk = response.getPk();
                lastTime = response.getTs();
                if ((response.hasIsFinish() && response.getIsFinish()) ||
                        (!response.hasIsFinish() && response.getCount() < client.getConfig().getTraverseLimit())) {
                    pid++;
                    if (pid >= th.getPartitions().length) {
                        isFinished = true;
                    }
                }
                return;
            }
            if (response != null) {
                throw new TabletException(response.getCode(), response.getMsg());
            }
            throw new TabletException("rtidb internal server error");
        } while (true);
    }

    @Override
    public int getCount() {
        throw new UnsupportedOperationException("getCount is not supported");
    }

    @Override
    public List<ColumnDesc> getSchema() {
        if (th != null && th.getSchemaMap().size() > 0) {
            return th.getSchemaMap().get(th.getSchema().size() + th.getSchemaMap().size());
        }
        return schema;
    }

    @Override
    public boolean valid() {
        if (offset <= totalSize) {
            return true;
        }
        return false;
    }

    @Override
    public long getKey() {
        return time;
    }

    @Override
    public String getPK() {
        return pk;
    }

    @Override
    public ByteBuffer getValue() {
        if (schema != null && !schema.isEmpty()) {
            throw new UnsupportedOperationException("getValue is not supported");
        }
        if (compressType == NS.CompressType.kSnappy) {
            byte[] data = new byte[slice.remaining()];
            slice.get(data);
            byte[] uncompressed = Compress.snappyUnCompress(data);
            return ByteBuffer.wrap(uncompressed);
        } else {
            return slice;
        }
    }

    @Override
    public Object[] getDecodedValue() throws TabletException {
        if (schema == null || schema.isEmpty()) {
            throw new UnsupportedOperationException("getDecodedValue is not supported");
        }
        switch (th.getFormatVersion()) {
            case 1:
            {
                Object[] row = new Object[schema.size()];
                getDecodedValue(row, 0, row.length);
                return row;
            }
            default:
            {
                Object[] row = new Object[schema.size() + th.getSchemaMap().size()];
                getDecodedValue(row, 0, row.length);
                return row;
            }
        }

    }

    @Override
    public void getDecodedValue(Object[] row, int start, int length) throws TabletException {
        if (schema == null || schema.isEmpty()) {
            throw new UnsupportedOperationException("getDecodedValue is not supported");
        }
        if (compressType == NS.CompressType.kSnappy) {
            byte[] data = new byte[slice.remaining()];
            slice.get(data);
            byte[] uncompressed = Compress.snappyUnCompress(data);
            if (uncompressed == null) {
                throw new TabletException("snappy uncompress error");
            }
            switch (th.getFormatVersion()) {
                case 1:
                    rv.read(ByteBuffer.wrap(uncompressed), row, 0, length);
                    break;
                default:
                    RowCodec.decode(ByteBuffer.wrap(uncompressed), schema, row, 0, length);
            }
        } else {
            switch (th.getFormatVersion()) {
                case 1:
                    rv.read(slice, row, 0, length);
                    break;
                default:
                    RowCodec.decode(slice, schema, row, start, length);
            }
        }
    }

    @Override
    public void next() {
        if (offset >= totalSize && !isFinished) {
            try {
                getData();
            } catch(Exception e){
                throw new RuntimeException(e.getMessage());
            }
        }
        if (offset + 8 > totalSize) {
            offset += 8;
            return;
        }
        slice = this.bb.slice().asReadOnlyBuffer().order(ByteOrder.LITTLE_ENDIAN);
        slice.position(offset);
        int total_size = slice.getInt();
        int pk_size = slice.getInt();
        time = slice.getLong();

        if (pk_size < 0 || total_size - 8 - pk_size < 0) {
            throw new RuntimeException("bad frame data");
        }
        byte[] pk_buf = new byte[pk_size];
        slice.get(pk_buf);
        pk = new String(pk_buf, charset);
        offset += (8 + total_size);
        slice.limit(offset);
    }
}
