package com._4paradigm.rtidb.client.ha.impl;

import com._4paradigm.rtidb.client.TabletException;
import com._4paradigm.rtidb.client.ha.*;
import com._4paradigm.rtidb.ns.NS;
import com._4paradigm.rtidb.ns.NS.PartitionMeta;
import com._4paradigm.rtidb.ns.NS.TableInfo;
import com._4paradigm.rtidb.ns.NS.TablePartition;
import com._4paradigm.rtidb.type.Type;
import com.google.protobuf.InvalidProtocolBufferException;
import io.brpc.client.*;
import org.apache.zookeeper.WatchedEvent;
import org.apache.zookeeper.Watcher;
import org.apache.zookeeper.ZooKeeper;
import org.apache.zookeeper.data.Stat;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import rtidb.api.TabletServer;
import rtidb.blobserver.BlobServer;

import java.io.IOException;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.util.*;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

public class RTIDBClusterClient implements Watcher, RTIDBClient {
    private final static Logger logger = LoggerFactory.getLogger(RTIDBClusterClient.class);
    private static Set<String> localIpAddr = new HashSet<String>();
    private final static ScheduledExecutorService clusterGuardThread = Executors.newScheduledThreadPool(1, new ThreadFactory() {
        public Thread newThread(Runnable r) {
            Thread t = Executors.defaultThreadFactory().newThread(r);
            t.setDaemon(true);
            return t;
        }
    });
    private volatile ZooKeeper zookeeper;
    private volatile Map<String, TableHandler> name2tables = new TreeMap<String, TableHandler>();
    private volatile Map<Integer, TableHandler> id2tables = new TreeMap<Integer, TableHandler>();
    private RpcBaseClient baseClient;
    private NodeManager nodeManager;
    private RTIDBClientConfig config;
    private Watcher notifyWatcher;
    private AtomicBoolean watching = new AtomicBoolean(true);
    private AtomicBoolean isClose = new AtomicBoolean(false);
    public RTIDBClusterClient(RTIDBClientConfig config) {
        this.config = config;
    }

    public void init() throws TabletException, IOException {
        RpcClientOptions options = new RpcClientOptions();
        options.setIoThreadNum(config.getIoThreadNum());
        options.setMaxConnectionNumPerHost(config.getMaxCntCnnPerHost());
        options.setReadTimeoutMillis(config.getReadTimeout());
        options.setWriteTimeoutMillis(config.getWriteTimeout());
        options.setMaxTryTimes(config.getMaxRetryCnt());
        options.setTimerBucketSize(config.getTimerBucketSize());
        baseClient = new RpcBaseClient(options);
        nodeManager = new NodeManager(baseClient);
        getLocalIpAddress();
        notifyWatcher = new Watcher() {
            @Override
            public void process(WatchedEvent event) {
                try {
                    refreshNodeList();
                    refreshRouteTable();
                } catch (Exception e) {
                    logger.error("fail to handle zookeeper connected  event", e);
                } finally {
                    try {
                        zookeeper.getData(config.getZkTableNotifyPath(), notifyWatcher, null);
                        watching.set(true);
                    } catch (Exception e) {
                        logger.error("fail to add watch to notify node", e);
                        watching.set(false);
                    }
                }
            }
            
        };
        isClose.set(false);
        connectToZk();
        onZkConnected();
        tryWatch();
        clusterGuardThread.schedule(new Runnable() {
            @Override
            public void run() {
                checkWatchStatus();
                
            }
        }, 1, TimeUnit.MINUTES);
        
    }
    
    private void connectToZk() throws TabletException,IOException{
        ZooKeeper localZk = new ZooKeeper(config.getZkEndpoints(), (int) config.getZkSesstionTimeout(), this);
        int failedCountDown = 20;
        while (!localZk.getState().isConnected() && failedCountDown > 0) {
            try {
                Thread.sleep(1000);
            } catch (InterruptedException e) {
                logger.error("interrupted", e);
            }
            failedCountDown--;
        }
        if (!localZk.getState().isConnected()) {
            try {
                localZk.close();
            } catch (Exception e) {
                logger.error("fail to close local zookeeper client", e);
            }
            throw new TabletException("fail to connect zookeeper " + config.getZkEndpoints());
        }
        ZooKeeper old = zookeeper;
        if (old != null) {
            try {
                old.close();
                logger.info("close old zookeeper client ok");
            }catch(Exception e) {
                logger.error("fail to close old zookeeper client", e);
            }
        }
        zookeeper = localZk;
        logger.info("switch to new zookeeper client instance ok");
    }

    private void onZkConnected() {
        try {
            refreshNodeList();
            refreshRouteTable();
        } catch (Exception e) {
            logger.error("fail to handle zookeeper connected  event", e);
        }
    }
    
    private void getLocalIpAddress() {
        try {
            Enumeration e = NetworkInterface.getNetworkInterfaces();
            while(e.hasMoreElements()){
                NetworkInterface n = (NetworkInterface) e.nextElement();
                Enumeration ee = n.getInetAddresses();
                while (ee.hasMoreElements()){
                    InetAddress i = (InetAddress) ee.nextElement();
                    logger.info("ip {} of local host binded ", i.getHostAddress());
                    localIpAddr.add(i.getHostAddress().toLowerCase());
                }
            }
            // get local hostname
            String hostname = InetAddress.getLocalHost().getHostName();
            localIpAddr.add(hostname);
            logger.info("add hostname {} to local ip set ", hostname);
        } catch (Exception e) {
            logger.error("fail to get local ip address", e);
        }
    }

    private void tryWatch() {
        try {
            if (zookeeper == null || !zookeeper.getState().isConnected()) {
                connectToZk();
                onZkConnected();
            } 
            zookeeper.getData(config.getZkTableNotifyPath(), notifyWatcher, null);
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
    
    public void refreshRouteTable() {
        Map<String, TableHandler> oldTables = name2tables;
        Map<String, TableHandler> newTables = new TreeMap<String, TableHandler>();
        Map<Integer, TableHandler> oldid2tables = id2tables;
        Map<Integer, TableHandler> newid2tables = new TreeMap<Integer, TableHandler>();
        try {
            Stat stat = zookeeper.exists(config.getZkTableRootPath(), false);
            if (stat == null) {
                logger.warn("path {} does not exist", config.getZkTableRootPath());
                return;
            }
            List<TableInfo> newTableList = new ArrayList<TableInfo>();
            List<String> children = zookeeper.getChildren(config.getZkTableRootPath(), false);
            for (String path : children) {
                byte[] data = zookeeper.getData(config.getZkTableRootPath() + "/" + path, false, null);
                if (data != null) {
                    try {
                        TableInfo tableInfo = TableInfo.parseFrom(data);
                        newTableList.add(tableInfo);
                    } catch (InvalidProtocolBufferException e) {
                        logger.error("invalid table data with name {}", path, e);
                    }
                }
            }

            for (TableInfo table : newTableList) {
                TableHandler handler = new TableHandler(table);
                if (config.getReadStrategies().containsKey(table.getName())) {
                    handler.setReadStrategy(config.getReadStrategies().get(table.getName()));
                } else {
                    handler.setReadStrategy(config.getGlobalReadStrategies());
                }
                PartitionHandler[] partitionHandlerGroup = new PartitionHandler[table.getTablePartitionList().size()];
                for (TablePartition partition : table.getTablePartitionList()) {
                    PartitionHandler ph = partitionHandlerGroup[partition.getPid()];
                    if (ph == null) {
                        ph = new PartitionHandler();
                        partitionHandlerGroup[partition.getPid()] = ph;
                    }
                    for (PartitionMeta pm : partition.getPartitionMetaList()) {
                        if (!pm.getIsAlive()) {
                            logger.warn("table {} partition {} with endpoint {} is dead", table.getName(), partition.getPid(), pm.getEndpoint());
                            continue;
                        }
                        EndPoint endpoint = new EndPoint(pm.getEndpoint());
                        BrpcChannelGroup bcg = nodeManager.getChannel(endpoint);
                        if (bcg == null) {
                            logger.warn("no alive endpoint for table {}, expect endpoint {}", table.getName(),
                                    endpoint);
                            continue;
                        }
                        SingleEndpointRpcClient client = new SingleEndpointRpcClient(baseClient);
                        client.updateEndpoint(endpoint, bcg);
                        TabletServer ts = (TabletServer) RpcProxy.getProxy(client, TabletServer.class);
                        if (pm.getIsLeader()) {
                            ph.setLeader(ts);
                        } else {
                            ph.getFollowers().add(ts);
                        }
                        if (localIpAddr.contains(endpoint.getIp().toLowerCase())) {
                            ph.setFastTablet(ts);
                        }
                    }
                }
                if (table.hasTableType()) {
                    List<String> blobs = new ArrayList<String>();
                    if (table.hasBlobInfo()) {
                        NS.BlobInfo blobInfo = table.getBlobInfo();
                        for (NS.BlobPartition part : blobInfo.getBlobPartitionList()) {
                            for (NS.BlobPartitionMeta meta : part.getPartitionMetaList()) {
                                if (!meta.getIsAlive()) {
                                    continue;
                                }
                                blobs.add(meta.getEndpoint());
                            }
                        }
                    }
                    if (blobs.size() > 0) {
                        EndPoint endPoint = new EndPoint(blobs.get(0));
                        BrpcChannelGroup bcg = nodeManager.getChannel(endPoint);
                        if (bcg == null) {
                            logger.warn("no alive endpoint for table {}, expect endpoint {}", table.getName(),
                                    endPoint);
                        } else {
                            SingleEndpointRpcClient client = new SingleEndpointRpcClient(baseClient);
                            client.updateEndpoint(endPoint, bcg);
                            BlobServer bs = (BlobServer) RpcProxy.getProxy(client, BlobServer.class);
                            handler.setBlobServer(bs);
                        }
                    }
                }

                handler.setPartitions(partitionHandlerGroup);
                newTables.put(table.getName(), handler);
                newid2tables.put(table.getTid(), handler);
            }
            // swap
            name2tables = newTables;
            id2tables = newid2tables;
            oldTables.clear();
            oldid2tables.clear();
        } catch (Exception e) {
            logger.error("fail to refresh table", e);
        }
    }

    public boolean refreshNodeList() {
        try {
            Stat stat = zookeeper.exists(config.getZkNodeRootPath(), false);
            if (stat == null) {
                logger.warn("path {} does not exist", config.getZkNodeRootPath());
                return true;
            }
            Set<EndPoint> endpoinSet = new HashSet<EndPoint>();
            List<String> children = zookeeper.getChildren(config.getZkNodeRootPath(), false);
            for (String path : children) {
                if (path.isEmpty()) {
                    continue;
                }
                logger.info("alive endpoint {}", path);
                String[] parts = path.split(":");
                if (parts.length != 2) {
                    logger.warn("invalid endpoint {}", path);
                    continue;
                }
                try {
                    endpoinSet.add(new EndPoint(parts[0], Integer.parseInt(parts[1])));
                } catch (Exception e) {
                    logger.error("fail to add endpoint", e);
                }
            }
            children.clear();;
            children = zookeeper.getChildren(config.getZkOssNodeRootPath(), false);
            for (String path : children) {
                if (path.isEmpty()) {
                    continue;
                }
                logger.info("alive blob endpoint {}", path);
                String[] parts = path.split(":");
                if (parts.length != 2) {
                    logger.warn("invalid blob endpoint {}", path);
                    continue;
                }
                try {
                    endpoinSet.add(new EndPoint(parts[0], Integer.parseInt(parts[1])));
                } catch (Exception e) {
                    logger.error("fail to add blob endpoint", e);
                }
            }
            nodeManager.update(endpoinSet);
            return true;
        } catch (Exception e) {
            logger.error("fail to refresh node manger", e);
        }
        return false;
    }

    public TableHandler getHandler(String name) {
        return name2tables.get(name);
    }

    public TableHandler getHandler(int id) {
        return id2tables.get(id);
    }

    @Override
    public void process(WatchedEvent event) {
    }

    @Override
    public void close() {
        isClose.set(true);
        // static members need not shutdown
        // clusterGuardThread.shutdown();
        if (zookeeper != null) {
            try {
                zookeeper.close();
            } catch (InterruptedException e) {
                logger.error("fail to close zk", e);
            }
        }
        if (nodeManager != null) {
            try {

                nodeManager.close();
            } catch (Exception e) {
                logger.error("fail to close node manager", e);
            }
        }
        if (baseClient != null) {
            try {
                baseClient.stop();
            } catch (Exception e) {
                logger.error("fail to close base client", e);
            }
        }
    }

    @Override
    public RTIDBClientConfig getConfig() {
        return config;
    }

}
