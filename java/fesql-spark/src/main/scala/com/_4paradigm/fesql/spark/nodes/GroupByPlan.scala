package com._4paradigm.fesql.spark.nodes

import com._4paradigm.fesql.spark.utils.SparkColumnUtil
import com._4paradigm.fesql.spark.{PlanContext, SparkInstance}
import com._4paradigm.fesql.vm.PhysicalGroupNode
import org.apache.spark.sql.Column

import scala.collection.mutable

object GroupByPlan {

  def gen(ctx: PlanContext, node: PhysicalGroupNode, input: SparkInstance): SparkInstance = {
    val inputDf = input.getDf(ctx.getSparkSession)

    val groupByExprs = node.group().keys()
    val groupByCols = mutable.ArrayBuffer[Column]()
    for (i <- 0 until groupByExprs.GetChildNum()) {
      val expr = groupByExprs.GetChild(i)

      val colIdx = SparkColumnUtil.resolveColumnIndex(expr, node.GetProducer(0))
      groupByCols += SparkColumnUtil.getColumnFromIndex(inputDf, colIdx)
    }

    val partitions = ctx.getConf("fesql.group.partitions", 0)
    val groupedDf = if (partitions > 0) {
      inputDf.repartition(partitions, groupByCols: _*)
    } else {
      inputDf.repartition(groupByCols: _*)
    }
    SparkInstance.fromDataFrame(groupedDf)
  }
}
