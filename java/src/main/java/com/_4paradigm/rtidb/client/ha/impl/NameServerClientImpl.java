package com._4paradigm.rtidb.client.ha.impl;

import com._4paradigm.rtidb.client.NameServerClient;
import com._4paradigm.rtidb.client.TabletException;
import com._4paradigm.rtidb.client.ha.RTIDBClientConfig;
import com._4paradigm.rtidb.client.schema.ColumnDesc;
import com._4paradigm.rtidb.client.schema.ColumnType;
import com._4paradigm.rtidb.client.schema.IndexDef;
import com._4paradigm.rtidb.client.schema.TableDesc;
import com._4paradigm.rtidb.client.type.DataType;
import com._4paradigm.rtidb.client.type.IndexType;
import com._4paradigm.rtidb.client.type.TableType;
import com._4paradigm.rtidb.common.Common;
import com._4paradigm.rtidb.ns.NS.*;
import com._4paradigm.rtidb.type.Type;
import io.brpc.client.*;
import org.apache.zookeeper.KeeperException;
import org.apache.zookeeper.WatchedEvent;
import org.apache.zookeeper.Watcher;
import org.apache.zookeeper.ZooKeeper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import rtidb.nameserver.NameServer;

import java.io.IOException;
import java.nio.charset.Charset;
import java.util.*;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

public class NameServerClientImpl implements NameServerClient, Watcher {
    private final static Logger logger = LoggerFactory.getLogger(NameServerClientImpl.class);
    private String zkEndpoints;
    private String leaderPath;
    private String severNamesPath;
    private String sdkEndpointPath;
    private volatile ZooKeeper zookeeper;
    private SingleEndpointRpcClient rpcClient;
    private volatile NameServer ns;
    private Watcher notifyWatcher;
    private AtomicBoolean watching = new AtomicBoolean(true);
    private AtomicBoolean isClose = new AtomicBoolean(false);
    private RTIDBClientConfig config;
    private RpcBaseClient bs = null;
    private static Map<String, Type.DataType> TYPE_MAPING = new HashMap<>();

    static {
        TYPE_MAPING.put("int16", Type.DataType.kSmallInt);
        TYPE_MAPING.put("int32", Type.DataType.kInt);
        TYPE_MAPING.put("int64", Type.DataType.kBigInt);
        TYPE_MAPING.put("float", Type.DataType.kFloat);
        TYPE_MAPING.put("double", Type.DataType.kDouble);
        TYPE_MAPING.put("string", Type.DataType.kVarchar);
        TYPE_MAPING.put("timestamp", Type.DataType.kTimestamp);
        TYPE_MAPING.put("bool", Type.DataType.kBool);
        TYPE_MAPING.put("date", Type.DataType.kDate);
    }

    private final static ScheduledExecutorService clusterGuardThread = Executors.newScheduledThreadPool(1, new ThreadFactory() {
        public Thread newThread(Runnable r) {
            Thread t = Executors.defaultThreadFactory().newThread(r);
            t.setDaemon(true);
            return t;
        }
    });

    public NameServerClientImpl(RTIDBClientConfig config) {
        this.zkEndpoints = config.getZkEndpoints();
        this.leaderPath = config.getZkRootPath() + "/leader";
        this.severNamesPath = config.getZkRootPath() + "/map/names";
        this.sdkEndpointPath = config.getZkRootPath() + "/map/sdkendpoints";
        this.config = config;
    }

    public NameServerClientImpl(String zkEndpoints, String leaderPath) {
        this.zkEndpoints = zkEndpoints;
        this.leaderPath = leaderPath;
        this.severNamesPath = "";
        this.sdkEndpointPath = "";
        this.config = null;
    }

    @Deprecated
    public NameServerClientImpl(String endpoint) {
        String realEp = endpoint;
        try {
            if (zookeeper.exists(this.sdkEndpointPath + "/" + realEp, false) != null) {
                // get sdkendpoint
                byte[] data1 = zookeeper.getData(this.sdkEndpointPath + "/" + realEp, false, null);
                if (data1 != null) {
                    realEp = new String(data1, Charset.forName("UTF-8"));
                }
            } else if (zookeeper.exists(this.severNamesPath + "/" + realEp, false) != null) {
                // get real endpoint
                byte[] data2 = zookeeper.getData(this.severNamesPath + "/" + realEp, false, null);
                if (data2 != null) {
                    realEp = new String(data2, Charset.forName("UTF-8"));
                }
            }
        } catch (KeeperException e) {
            e.printStackTrace();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        EndPoint addr = new EndPoint(realEp);
        bs = new RpcBaseClient();
        rpcClient = new SingleEndpointRpcClient(bs);
        BrpcChannelGroup bcg = new BrpcChannelGroup(addr.getIp(), addr.getPort(),
                bs.getRpcClientOptions().getMaxConnectionNumPerHost(), bs.getBootstrap());
        rpcClient.updateEndpoint(addr, bcg);
        ns = (NameServer) RpcProxy.getProxy(rpcClient, NameServer.class);
    }

    public void init() throws Exception {
        isClose.set(false);
        notifyWatcher = new Watcher() {
            @Override
            public void process(WatchedEvent event) {
                logger.info("zookeeper leader node has changed");
                try {
                    connectNS();
                } catch (Exception e) {
                    logger.error("fail to handle zookeeper node update event", e);
                } finally {
                    try {
                        zookeeper.getChildren(leaderPath, notifyWatcher);
                        watching.set(true);
                    } catch (Exception e) {
                        logger.error("fail to add watch to notify node", e);
                        watching.set(false);
                    }
                }
            }
        };
        if (bs == null) {
            if (config != null) {
                RpcClientOptions options = new RpcClientOptions();
                options.setIoThreadNum(config.getIoThreadNum());
                options.setMaxConnectionNumPerHost(config.getMaxCntCnnPerHost());
                options.setReadTimeoutMillis(config.getReadTimeout());
                options.setWriteTimeoutMillis(config.getWriteTimeout());
                options.setMaxTryTimes(config.getMaxRetryCnt());
                options.setTimerBucketSize(config.getTimerBucketSize());
                bs = new RpcBaseClient(options);
            } else {
                bs = new RpcBaseClient();
            }

        }
        connectZk();
        connectNS();
        tryWatch();
        clusterGuardThread.schedule(new Runnable() {
            @Override
            public void run() {
                checkWatchStatus();
            }
        }, 1, TimeUnit.MINUTES);
    }

    private void connectZk() throws TabletException, IOException {
        ZooKeeper localZk = new ZooKeeper(zkEndpoints, 10000, this);
        int tryCnt = 20;
        while (!localZk.getState().isConnected() && tryCnt > 0) {
            try {
                Thread.sleep(1000);
            } catch (InterruptedException e) {
                logger.error("interrupted", e);
            }
            tryCnt--;
        }
        if (!localZk.getState().isConnected()) {
            try {
                localZk.close();
            } catch (Exception e) {
                logger.error("fail to close local zookeeper client", e);
            }
            throw new TabletException("fail to connect to zookeeper " + zkEndpoints);
        }
        ZooKeeper old = zookeeper;
        if (old != null) {
            try {
                old.close();
                logger.info("close old zookeeper client ok");
            } catch (Exception e) {
                logger.error("fail to close old zookeeper client", e);
            }
        }
        zookeeper = localZk;
        logger.info("switch to new zookeeper client instance ok");
    }

    private void connectNS() throws Exception {
        List<String> children = zookeeper.getChildren(leaderPath, false);
        if (children.isEmpty()) {
            throw new TabletException("no nameserver avaliable");
        }
        Collections.sort(children);
        byte[] bytes = zookeeper.getData(leaderPath + "/" + children.get(0), false, null);
        String realEp = new String(bytes, Charset.forName("UTF-8"));

        if (zookeeper.exists(this.sdkEndpointPath + "/" + realEp, false) != null) {
            // get sdkendpoint
            byte[] data1 = zookeeper.getData(this.sdkEndpointPath + "/" + realEp, false, null);
            if (data1 != null) {
                realEp = new String(data1, Charset.forName("UTF-8"));
            }
        } else if (zookeeper.exists(this.severNamesPath + "/" + realEp, false) != null) {
            // get real endpoint
            byte[] data2 = zookeeper.getData(this.severNamesPath + "/" + realEp, false, null);
            if (data2 != null) {
                realEp = new String(data2, Charset.forName("UTF-8"));
            }
        }

        EndPoint endpoint = new EndPoint(realEp);
        if (rpcClient != null) {
            rpcClient.stop();
        }
        rpcClient = new SingleEndpointRpcClient(bs);
        BrpcChannelGroup bcg = new BrpcChannelGroup(endpoint.getIp(), endpoint.getPort(),
                bs.getRpcClientOptions().getMaxConnectionNumPerHost(), bs.getBootstrap());
        rpcClient.updateEndpoint(endpoint, bcg);
        ns = (NameServer) RpcProxy.getProxy(rpcClient, NameServer.class);
        logger.info("connect leader path {} endpoint {} ok", children.get(0), endpoint);
    }

    private void tryWatch() {
        try {
            if (zookeeper == null || !zookeeper.getState().isConnected()) {
                connectZk();
                connectNS();
            }
            zookeeper.getChildren(leaderPath, notifyWatcher);
            watching.set(true);
        } catch (Exception e) {
            logger.error("fail to add watch to notify node and will retry one minute later", e);
            watching.set(false);
        }
    }

    private void checkWatchStatus() {
        if (isClose.get()) {
            return;
        }
        if (!watching.get()) {
            tryWatch();
        }
        clusterGuardThread.schedule(new Runnable() {

            @Override
            public void run() {
                checkWatchStatus();
            }
        }, 1, TimeUnit.MINUTES);
    }


    @Override
    public boolean createTable(TableDesc tableDesc) {
        TableInfo.Builder builder = TableInfo.newBuilder();
        builder.setName(tableDesc.getName())
                .setTableType(TableType.valueFrom(tableDesc.getTableType()))
                .setPartitionNum(1)
                .setReplicaNum(1)
                .setStorageMode(Common.StorageMode.kHDD)
                .setCompressType(CompressType.kNoCompress);
        for (ColumnDesc col : tableDesc.getColumnDescList()) {
            DataType dataType = col.getDataType();

            Common.ColumnDesc cd = Common.ColumnDesc.newBuilder()
                    .setName(col.getName())
                    .setDataType(DataType.valueFrom(dataType))
                    .setNotNull(col.isNotNull())
                    .build();
            builder.addColumnDescV1(cd);
        }
        String indexName = "";
        if (tableDesc.getIndexs() == null ||
                tableDesc.getIndexs().isEmpty()) {
            logger.warn("no index");
            return false;
        }
        for (IndexDef index : tableDesc.getIndexs()) {
            if (index.getIndexType() == IndexType.PrimaryKey ||
                    index.getIndexType() == IndexType.AutoGen ||
                    index.getIndexType() == IndexType.Unique ||
                    index.getIndexType() == IndexType.NoUnique) {
                if (index.getIndexType() == IndexType.AutoGen) {
                    indexName = index.getIndexName();
                }
                Common.ColumnKey.Builder colKeyBuilder = Common.ColumnKey.newBuilder();
                colKeyBuilder.setIndexName(index.getIndexName())
                        .setIndexType(IndexType.valueFrom(index.getIndexType()));
                for (String colName : index.getColNameList()) {
                    colKeyBuilder.addColName(colName);
                }
                Common.ColumnKey columnKey = colKeyBuilder.build();
                builder.addColumnKey(columnKey);
            } else {
                //TODO: other index type
            }
        }
        TableInfo tableInfo = builder.build();
        if (!indexName.isEmpty()) {
            for (int i = 0; i < tableInfo.getColumnDescV1List().size(); i++) {
                Common.ColumnDesc columnDesc = tableInfo.getColumnDescV1List().get(i);
                if (columnDesc.getName().equals(indexName)) {
                    if (!columnDesc.getDataType().equals(DataType.valueFrom(DataType.BigInt))) {
                        logger.warn("autoGenPk column dataType must be BigInt");
                        return false;
                    }
                    break;
                }
            }
        }
        CreateTableRequest request = CreateTableRequest.newBuilder().setTableInfo(tableInfo).build();
        GeneralResponse response = ns.createTable(request);
        if (response != null && response.getCode() == 0) {
            return true;
        } else if (response != null) {
            logger.warn("fail to create table for error {}", response.getMsg());
        }
        return false;
    }

    @Override
    public boolean createTable(TableInfo tableInfo) {
        CreateTableRequest request = CreateTableRequest.newBuilder().setTableInfo(tableInfo).build();
        GeneralResponse response = ns.createTable(request);
        if (response != null && response.getCode() == 0) {
            return true;
        } else if (response != null) {
            logger.warn("fail to create table for error {}", response.getMsg());
        }
        return false;
    }

    @Override
    public boolean dropTable(String tname) {
        DropTableRequest request = DropTableRequest.newBuilder().setName(tname).build();
        GeneralResponse response = ns.dropTable(request);
        if (response != null && response.getCode() == 0) {
            return true;
        } else if (response != null) {
            logger.warn("fail to drop table for error {}", response.getMsg());
        }
        return false;
    }

    @Override
    public List<TableInfo> showTable(String tname) {
        ShowTableRequest request = null;
        if (tname == null || tname.isEmpty()) {
            request = ShowTableRequest.newBuilder().build();
        } else {
            request = ShowTableRequest.newBuilder().setName(tname).build();
        }
        ShowTableResponse response = ns.showTable(request);
        return response.getTableInfoList();
    }

    @Override
    public boolean addTableField(String tableName, String columnName, String columnType) {
        try {
            ColumnType.valueFrom(columnType);
        } catch (Exception e) {
            logger.warn(e.getMessage());
            return false;
        }
        Common.ColumnDesc columnDesc = Common.ColumnDesc.newBuilder()
                .setName(columnName)
                .setType(columnType)
                .build();
        AddTableFieldRequest request = AddTableFieldRequest.newBuilder()
                .setName(tableName)
                .setColumnDesc(columnDesc)
                .build();
        GeneralResponse response = ns.addTableField(request);
        if (response != null && response.getCode() == 0) {
            return true;
        } else if (response != null) {
            logger.warn("fail to add table field for error {}", response.getMsg());
        }
        return false;
    }

    @Override
    public void process(WatchedEvent event) {


    }

    public void close() {
        isClose.set(true);
        try {
            if (zookeeper != null) {
                zookeeper.close();
            }

        } catch (Exception e) {
            logger.error("fail to close zookeeper", e);
        }
        try {
            if (bs != null) {
                bs.stop();
            }
        } catch (Exception e) {
            logger.error("fail to close bs client", e);
        }
        try {
            if (rpcClient != null) {
                rpcClient.stop();
            }
        } catch (Exception e) {
            logger.error("fail to close rpc client ", e);
        }

    }

    @Override
    public boolean changeLeader(String tname, int pid) {
        ChangeLeaderRequest request = ChangeLeaderRequest.newBuilder().setName(tname).setPid(pid).build();
        GeneralResponse response = ns.changeLeader(request);
        if (response != null && response.getCode() == 0) {
            return true;
        }
        return false;
    }

    @Override
    public boolean recoverEndpoint(String endpoint) {
        RecoverEndpointRequest request = RecoverEndpointRequest.newBuilder().setEndpoint(endpoint).build();
        GeneralResponse response = ns.recoverEndpoint(request);
        if (response != null && response.getCode() == 0) {
            return true;
        }
        return false;
    }

    @Override
    public List<String> showTablet() {
        ShowTabletRequest request = ShowTabletRequest.newBuilder().build();
        ShowTabletResponse response = ns.showTablet(request);
        List<String> tablets = new ArrayList<String>();
        for (TabletStatus ts : response.getTabletsList()) {
            tablets.add(ts.getEndpoint());
        }
        return tablets;
    }

    public Map<String, String> showNs() throws Exception {
        List<String> node = zookeeper.getChildren(leaderPath, false);
        Map<String, String> nsEndpoint = new HashMap<>();
        int i = 0;
        if (node.isEmpty()) {
            return nsEndpoint;
        }
        Collections.sort(node);
        for (String e : node) {
            byte[] bytes = zookeeper.getData(leaderPath + "/" + e, false, null);
            String endpoint = new String(bytes);
            if (!nsEndpoint.containsKey(endpoint)) {
                if (i == 0) {
                    nsEndpoint.put(endpoint, "leader");
                    i++;
                } else {
                    nsEndpoint.put(endpoint, "standby");
                }
            }
        }
        return nsEndpoint;
    }

}
