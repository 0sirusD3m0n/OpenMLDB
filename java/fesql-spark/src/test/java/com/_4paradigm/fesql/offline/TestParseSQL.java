package com._4paradigm.fesql.offline;

import com._4paradigm.fesql.LibraryLoader;
import com._4paradigm.fesql.base.BaseStatus;
import com._4paradigm.fesql.type.TypeOuterClass;
import com._4paradigm.fesql.type.TypeOuterClass.TableDef;
import com._4paradigm.fesql.type.TypeOuterClass.ColumnDef;
import com._4paradigm.fesql.type.TypeOuterClass.Database;
import com._4paradigm.fesql.vm.*;
import org.junit.Test;

import java.nio.ByteBuffer;

import static org.junit.Assert.assertTrue;


public class TestParseSQL {

    @Test
    public void testParseSQL() {
        LibraryLoader.loadLibrary("fesql_jsdk_core");
        ColumnDef col1 = ColumnDef.newBuilder()
                .setName("col_1").setType(TypeOuterClass.Type.kDouble).build();
        ColumnDef col2 = ColumnDef.newBuilder()
                .setName("col_2").setType(TypeOuterClass.Type.kInt32).build();
        TableDef table = TableDef.newBuilder()
                .setName("t1")
                .addColumns(col1)
                .addColumns(col2)
                .build();
        Database db = Database.newBuilder()
                .setName("db")
                .addTables(table).build();

        Engine.InitializeGlobalLLVM();

        SimpleCatalog catalog = new SimpleCatalog();
        catalog.AddDatabase(db);

        BatchRunSession sess = new BatchRunSession();

        EngineOptions options = new EngineOptions();
        options.set_keep_ir(true);

        BaseStatus status = new BaseStatus();
        Engine engine = new Engine(catalog, options);
        assertTrue(engine.Get("select col_1, col_2 from t1;", "db", sess, status));
        CompileInfo compileInfo = sess.GetCompileInfo();

        long size = compileInfo.get_ir_size();
        ByteBuffer buffer = ByteBuffer.allocateDirect(Long.valueOf(size).intValue());
        compileInfo.get_ir_buffer(buffer);
        System.err.println("Dumped module string: len=" + size);

        PhysicalOpNode root = sess.GetPhysicalPlan();
        root.Print();

        FeSQLJITWrapper jit = new FeSQLJITWrapper();
        jit.Init();
        jit.AddModuleFromBuffer(buffer);

        engine.delete();
        jit.delete();
        options.delete();
        status.delete();
        sess.delete();
        catalog.delete();
    }

}
