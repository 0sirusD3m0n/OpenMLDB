package com._4paradigm.rtidb.client.ut.ha;

import com._4paradigm.rtidb.client.ha.PartitionHandler;
import com._4paradigm.rtidb.client.ha.TableHandler;
import com._4paradigm.rtidb.client.ha.TabletServerWapper;
import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.Test;
import rtidb.api.TabletServer;

import java.util.ArrayList;
import java.util.List;

public class ReadStrategyTest {
    private static PartitionHandler partitionHandler = new PartitionHandler();
    private static TabletServer leader;
    private static TabletServer follower1;
    private static TabletServer follower2;

    private static TabletServerWapper leaderW = new TabletServerWapper("leader", leader);
    private static TabletServerWapper follower1W = new TabletServerWapper("follower1", follower1);
    private static TabletServerWapper follower2W = new TabletServerWapper("follower2", follower2);
    private static List<TabletServerWapper> followers = new ArrayList<>();

    @BeforeClass
    public static void init() {
        partitionHandler.setLeader(leaderW);
        followers.add(follower1W);
        followers.add(follower2W);
        partitionHandler.setFollowers(followers);
    }

    //three replica
    @Test
    public void testReadLeaderForThree() {
        TabletServer tabletServer = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadLeader);
        Assert.assertTrue(tabletServer == leader);
    }

    @Test
    public void testReadFollowerForThree() {
        //followers.size()>0
        TabletServer tabletServer = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadFollower);
        Assert.assertTrue(tabletServer == follower1 || tabletServer == follower2);

        partitionHandler.getFollowers().remove(follower1);
        TabletServer tabletServer1 = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadFollower);
        Assert.assertTrue(tabletServer1 == follower2);
        partitionHandler.getFollowers().add(follower1W);

        partitionHandler.getFollowers().remove(follower2W);
        TabletServer tabletServer2 = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadFollower);
        Assert.assertTrue(tabletServer2 == follower1);
        partitionHandler.getFollowers().add(follower2W);
        //followers.size()==0
        partitionHandler.setFollowers(new ArrayList<TabletServerWapper>());
        Assert.assertTrue(tabletServer1 == leader);
        //reset
        partitionHandler.setFollowers(followers);
    }

    @Test
    public void testReadLocalForThree() {
        //fastTablet != null
        partitionHandler.setFastTablet(follower1W);
        TabletServer tabletServer = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadLocal);
        Assert.assertTrue(tabletServer == follower1);
        //fastTablet == null && followers.size() > 0
        partitionHandler.setFastTablet(null);
        TabletServer tabletServer1 = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadLocal);
        Assert.assertTrue(tabletServer == follower1 || tabletServer == follower2);
        //fastTablet == null && followers.size() == 0
        partitionHandler.setFollowers(new ArrayList<TabletServerWapper>());
        TabletServer tabletServer2 = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadLocal);
        Assert.assertTrue(tabletServer == leader);
        //reset
        partitionHandler.setFollowers(followers);
    }

    @Test
    public void testReadRandomForThree() {
        //followers.size() == 0
        partitionHandler.setFollowers(new ArrayList<TabletServerWapper>());
        TabletServer tabletServer = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadRandom);
        Assert.assertTrue(tabletServer == leader);
        //followers.size() > 0
        partitionHandler.setFollowers(followers);
        int leader_count = 0;
        int follower1_count = 0;
        int follower2_count = 0;
        for (int i = 0; i < 100; i++) {
            TabletServer tabletServer1 = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadRandom);
            if (tabletServer1 == leader) leader_count++;
            if (tabletServer1 == follower1) follower1_count++;
            if (tabletServer1 == follower2) follower2_count++;
        }
        Assert.assertTrue(leader_count > 0 && follower1_count > 0 && follower2_count > 0);
    }

    //two replica
    @Test
    public void testReadLeaderForTwo() {
        partitionHandler.getFollowers().remove(follower1);
        TabletServer tabletServer = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadLeader);
        Assert.assertTrue(tabletServer == leader);
    }

    @Test
    public void testReadFollowerForTwo() {
        //followers.size()>0
        TabletServer tabletServer = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadFollower);
        Assert.assertTrue(tabletServer == follower2);
        //followers.size()==0
        partitionHandler.setFollowers(new ArrayList<TabletServerWapper>());
        TabletServer tabletServer1 = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadFollower);
        Assert.assertTrue(tabletServer1 == leader);
        //reset
        partitionHandler.setFollowers(followers);
    }

    @Test
    public void testReadRandomForTwo() {
        //followers.size() == 0
        partitionHandler.setFollowers(new ArrayList<TabletServerWapper>());
        TabletServer tabletServer = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadRandom);
        Assert.assertTrue(tabletServer == leader);
        //followers.size() > 0
        partitionHandler.setFollowers(followers);
        int leader_count = 0;
        int follower1_count = 0;
        for (int i = 0; i < 100; i++) {
            TabletServer tabletServer1 = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadRandom);
            if (tabletServer1 == leader) leader_count++;
            if (tabletServer1 == follower1) follower1_count++;
        }
        Assert.assertTrue(leader_count > 0 && follower1_count > 0 );
    }

    @Test
    public void testReadLocalForTwo() {
        //fastTablet != null
        partitionHandler.setFastTablet(follower1W);
        TabletServer tabletServer = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadLocal);
        Assert.assertTrue(tabletServer == follower1);
        //fastTablet == null && followers.size() > 0
        partitionHandler.setFastTablet(null);
        TabletServer tabletServer1 = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadLocal);
        Assert.assertTrue(tabletServer == follower2);
        //fastTablet == null && followers.size() == 0
        partitionHandler.setFollowers(new ArrayList<TabletServerWapper>());
        TabletServer tabletServer2 = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadLocal);
        Assert.assertTrue(tabletServer == leader);
        //reset
        partitionHandler.setFollowers(followers);
    }

    //one replica
    @Test
    public void testReadLeaderForOne() {
        partitionHandler.getFollowers().remove(follower2);
        TabletServer tabletServer = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadLeader);
        Assert.assertTrue(tabletServer == leader);
    }

    @Test
    public void testReadFollowerForOne() {
        TabletServer tabletServer = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadFollower);
        Assert.assertTrue(tabletServer == leader);
    }

    @Test
    public void testReadRandomForOne() {
        TabletServer tabletServer = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadRandom);
        Assert.assertTrue(tabletServer == leader);
    }

    @Test
    public void testReadLocalForOne() {
        TabletServer tabletServer = partitionHandler.getReadHandler(TableHandler.ReadStrategy.kReadLocal);
        Assert.assertTrue(tabletServer == leader);
    }
}
