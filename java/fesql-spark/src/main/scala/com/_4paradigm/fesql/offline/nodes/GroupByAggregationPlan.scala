package com._4paradigm.fesql.offline.nodes

import com._4paradigm.fesql.offline.nodes.RowProjectPlan.ProjectConfig
import com._4paradigm.fesql.offline.utils.{FesqlUtil, SparkColumnUtil}
import com._4paradigm.fesql.offline.{PlanContext, SparkInstance, SparkRowCodec}
import com._4paradigm.fesql.vm.{CoreAPI, GroupbyInterface, PhysicalGroupAggrerationNode}
import com._4paradigm.fesql.codec.{Row => NativeRow}
import com._4paradigm.fesql.offline.JITManager
import org.apache.spark.sql.{Column, Row}
import scala.collection.mutable


object GroupByAggregationPlan {

  def gen(ctx: PlanContext, node: PhysicalGroupAggrerationNode, input: SparkInstance): SparkInstance = {
    val inputDf = input.getDf(ctx.getSparkSession)

    // Get parition keys
    val groupByExprs = node.getGroup_.keys()
    val groupByCols = mutable.ArrayBuffer[Column]()
    val groupIdxs = mutable.ArrayBuffer[Int]()
    for (i <- 0 until groupByExprs.GetChildNum()) {
      val expr = groupByExprs.GetChild(i)
      val colIdx = SparkColumnUtil.resolveColumnIndex(expr, node.GetProducer(0))
      groupIdxs += colIdx
      groupByCols += SparkColumnUtil.getCol(inputDf, colIdx)
    }

    // Sort by partition keys
    val sortedInputDf = inputDf.sortWithinPartitions(groupByCols:_*)

    // Get schema info
    val inputSchemaSlices = FesqlUtil.getOutputSchemaSlices(node.GetProducer(0))
    val inputSchema = FesqlUtil.getSparkSchema(node.GetProducer(0).GetOutputSchema())
    val outputSchemaSlices = FesqlUtil.getOutputSchemaSlices(node)
    val outputSchema = FesqlUtil.getSparkSchema(node.GetOutputSchema())

    // Wrap spark closure
    val limitCnt = node.GetLimitCnt
    val projectConfig = ProjectConfig(
      functionName = node.project().fn_name(),
      moduleTag = ctx.getTag,
      moduleBroadcast = ctx.getModuleBufferBroadcast,
      inputSchemaSlices = inputSchemaSlices,
      outputSchemaSlices = outputSchemaSlices,
      inputSchema = inputSchema
    )
    val groupKeyComparator = FesqlUtil.createGroupKeyComparator(groupIdxs.toArray, inputSchema)

    // Map partition
    val resultRDD = sortedInputDf.rdd.mapPartitions(iter => {
      val resultRowList =  mutable.ArrayBuffer[Row]()

      if (!iter.isEmpty) { // Ignore the empty partition
        var currentLimitCnt = 0

        // Init JIT
        val tag = projectConfig.moduleTag
        val buffer = projectConfig.moduleBroadcast.value.getBuffer
        JITManager.initJITModule(tag, buffer)
        val jit = JITManager.getJIT(tag)
        val fn = jit.FindFunction(projectConfig.functionName)

        val encoder = new SparkRowCodec(projectConfig.inputSchemaSlices)
        val decoder = new SparkRowCodec(projectConfig.outputSchemaSlices)

        val inputFesqlSchema = FesqlUtil.getFeSQLSchema(projectConfig.inputSchema)
        val outputFields = projectConfig.outputSchemaSlices.map(_.size).sum

        // Init first groupby interface
        var groupbyInterface = new GroupbyInterface(inputFesqlSchema)
        var lastRow: Row = null
        val grouopNativeRows =  mutable.ArrayBuffer[NativeRow]()

        iter.foreach(row => {
          if (limitCnt <= 0 || currentLimitCnt < limitCnt) { // Do not set limit or not reach the limit
            if (lastRow != null) { // Ignore the first row in partition
              val groupChanged = groupKeyComparator.apply(row, lastRow)
              if (groupChanged) {
                // Run group by for the same group
                val outputFesqlRow = CoreAPI.GroupbyProject(fn, groupbyInterface)
                val outputArr = Array.fill[Any](outputFields)(null)
                decoder.decode(outputFesqlRow, outputArr)
                resultRowList += Row.fromSeq(outputArr)
                currentLimitCnt += 1

                // Reset group interface and release native rows
                groupbyInterface.delete()
                groupbyInterface = new GroupbyInterface(inputFesqlSchema)
                grouopNativeRows.map(nativeRow => nativeRow.delete())
                grouopNativeRows.clear()
              }
            }

            // Buffer the row in the same group
            lastRow = row
            val nativeInputRow = encoder.encode(row)
            groupbyInterface.AddRow(nativeInputRow)
            grouopNativeRows += nativeInputRow
          }
        })

        // Run group by for the last group
        if (limitCnt <= 0 || currentLimitCnt < limitCnt) {
          val outputFesqlRow = CoreAPI.GroupbyProject(fn, groupbyInterface)
          val outputArr = Array.fill[Any](outputFields)(null)
          decoder.decode(outputFesqlRow, outputArr)
          resultRowList += Row.fromSeq(outputArr)
        }

        groupbyInterface.delete()
        grouopNativeRows.map(nativeRow => nativeRow.delete())
        grouopNativeRows.clear()
      }

      resultRowList.toIterator
    })

    SparkInstance.fromRDD(outputSchema, resultRDD)
  }

}
