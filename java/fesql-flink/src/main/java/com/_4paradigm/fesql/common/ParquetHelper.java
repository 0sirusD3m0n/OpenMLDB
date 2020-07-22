package com._4paradigm.fesql.common;

import org.apache.flink.formats.parquet.ParquetRowInputFormat;
import org.apache.flink.streaming.api.datastream.DataStream;
import org.apache.flink.streaming.api.environment.StreamExecutionEnvironment;
import org.apache.flink.streaming.api.functions.source.FileProcessingMode;
import org.apache.flink.types.Row;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.LocatedFileStatus;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.fs.RemoteIterator;
import org.apache.parquet.hadoop.ParquetFileReader;
import org.apache.parquet.hadoop.util.HadoopInputFile;
import org.apache.parquet.schema.MessageType;
import java.io.IOException;
import java.net.URI;
import java.util.ArrayList;
import java.util.List;

public class ParquetHelper {

	public static DataStream<Row> ParquetHelper(final StreamExecutionEnvironment env, String source) {
		MessageType schema = readParquetSchema(source);
		ParquetRowInputFormat parquetRowInputFormat = new ParquetRowInputFormat(new org.apache.flink.core.fs.Path(source), schema);
		return env.readFile(parquetRowInputFormat, source, FileProcessingMode.PROCESS_ONCE, 10);
	}

	public static MessageType readParquetSchema(String path) {
		Configuration _default = new Configuration();
		List<LocatedFileStatus> files = listFiles(_default, path, null);
		if (files.isEmpty()) {
			throw new RuntimeException(String.format("Empty path [%s] ", path));
		}
		Path headPath = files.get(0).getPath();
		try (ParquetFileReader r = ParquetFileReader.open(HadoopInputFile.fromPath(headPath, _default))) {
			return r.getFooter().getFileMetaData().getSchema();
		} catch (IOException e) {
			throw new RuntimeException(String.format("Error read Parquet file schema, path [%s].", headPath), e);
		}
	}

	public static List<LocatedFileStatus> listFiles(Configuration conf, String path, String ends) {
		List<LocatedFileStatus> files = new ArrayList<>();
		try {
			FileSystem fs = FileSystem.get(new URI(path), conf);
			RemoteIterator<LocatedFileStatus> fileStatus = fs.listFiles(new Path(path), true);
			if (fileStatus != null) {
				while (fileStatus.hasNext()) {
					LocatedFileStatus cur = fileStatus.next();
					if (cur.isFile()) {
						String fileName = cur.getPath().getName();
						if (fileName.startsWith(".") || fileName.startsWith("_")) {
							continue;
						}
						if (ends == null || fileName.endsWith(ends)) {
							files.add(cur);
						}
					}
				}
			}
		} catch (Exception e) {
			System.out.println(e.toString());
		}
		return files;
	}
}
