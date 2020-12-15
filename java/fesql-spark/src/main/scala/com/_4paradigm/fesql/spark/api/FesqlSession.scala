package com._4paradigm.fesql.spark.api

import com._4paradigm.fesql.spark.SparkPlanner
import com._4paradigm.fesql.spark.element.FesqlConfig
import org.apache.commons.io.IOUtils
import org.slf4j.LoggerFactory
import org.apache.spark.sql.{DataFrame, SparkSession}
import org.apache.spark.SparkConf

import scala.collection.mutable

/**
 * The class to provide SparkSession-like API.
 */
class FesqlSession {

  private val logger = LoggerFactory.getLogger(this.getClass)

  private var sparkSession: SparkSession = null
  private var sparkMaster: String = null

  private val registeredTables = mutable.HashMap[String, DataFrame]()
  private var configs: mutable.HashMap[String, Any] = _
  private var scalaConfig: Map[String, Any] = Map()

  /**
   * Construct with Spark session.
   *
   * @param sparkSession
   */
  def this(sparkSession: SparkSession, config: mutable.HashMap[String, Any]) = {
    this()
    this.sparkSession = sparkSession
    this.sparkSession.conf.set(FesqlConfig.configTimeZone, FesqlConfig.timeZone)
    this.configs = config

    for ((k, v) <- configs) {
      scalaConfig += (k -> v)
      k match {
        case "fesql.skew.ratio" => FesqlConfig.skewRatio = v.asInstanceOf[Double]
        case "fesql.skew.level" => FesqlConfig.skewLevel = v.asInstanceOf[Int]
        case "fesql.skew.watershed" => FesqlConfig.skewCnt = v.asInstanceOf[Int]
        case "fesql.skew.cnt.name" => FesqlConfig.skewCntName = v.asInstanceOf[String]
        case "fesql.skew.tag" => FesqlConfig.skewTag = v.asInstanceOf[String]
        case "fesql.skew.position" => FesqlConfig.skewPosition = v.asInstanceOf[String]
        case "fesql.mode" => FesqlConfig.mode = v.asInstanceOf[String]
        case "fesql.group.partitions" => FesqlConfig.paritions = v.asInstanceOf[Int]
        case "fesql.timeZone" => FesqlConfig.timeZone = v.asInstanceOf[String]
      }
    }
  }

  /**
   * Construct with Spark master string.
   *
   *
   */
//  def this(sparkMaster: String) = {
//    this()
//    this.sparkMaster = sparkMaster
//  }

  /**
   * Get or create the Spark session.
   *
   * @return
   */
  def getSparkSession: SparkSession = {
    this.synchronized {
      if (this.sparkSession == null) {

        if (this.sparkMaster == null) {
          this.sparkMaster = "local"
        }

        logger.debug(s"Create new SparkSession with master=${this.sparkMaster}")
        val sparkConf = new SparkConf()
        val sparkMaster = sparkConf.get("spark.master", this.sparkMaster)
        val builder = SparkSession.builder()

        // TODO: Need to set for official Spark 2.3.0 jars
        logger.debug("Set spark.hadoop.yarn.timeline-service.enabled as false")
        builder.config(FesqlConfig.configSparkEnable, value = false)

        this.sparkSession = builder.appName("FesqlApp")
          .master(sparkMaster)
          .getOrCreate()
      }

      this.sparkSession
    }
  }

  /**
   * Read the file with get dataframe with Spark API.
   *
   * @param filePath
   * @param format
   * @return
   */
  def read(filePath: String, format: String = "parquet"): FesqlDataframe = {
    val spark = this.getSparkSession

    val sparkDf = format match {
      case "parquet" => spark.read.parquet(filePath)
      case "csv" => spark.read.csv(filePath)
      case "json" => spark.read.json(filePath)
      case "text" => spark.read.text(filePath)
      case "orc" => spark.read.orc(filePath)
      case _ => null
    }

    new FesqlDataframe(this, sparkDf)
  }

  /**
   * Read the Spark dataframe to Fesql dataframe.
   *
   * @param sparkDf
   * @return
   */
  def readSparkDataframe(sparkDf: DataFrame): FesqlDataframe = {
    new FesqlDataframe(this, sparkDf)
  }

  /**
   * Run sql with FESQL.
   *
   * @param sqlText
   * @return
   */
  def fesql(sqlText: String): FesqlDataframe = {
    var sql: String = sqlText
    if (!sql.trim.endsWith(";")) {
      sql = sql.trim + ";"
    }

    val planner = new SparkPlanner(getSparkSession, scalaConfig)
    val df = planner.plan(sql, registeredTables.toMap).getDf(getSparkSession)
    new FesqlDataframe(this, df)
  }

  /**
   * Run sql with FESQL.
   *
   * @param sqlText
   * @return
   */
  def sql(sqlText: String): FesqlDataframe = {
    fesql(sqlText)
  }

  /**
   * Run sql with Spark SQL API.
   *
   * @param sqlText
   * @return
   */
  def sparksql(sqlText: String): FesqlDataframe = {
    new FesqlDataframe(this, getSparkSession.sql(sqlText))
  }

  /**
   * Get the version from git commit message.
   */
  def version(): Unit = {
    val stream = this.getClass.getClassLoader.getResourceAsStream("fesql_git.properties")
    if (stream == null) {
      logger.warn("Project version is missing")
    } else {
      IOUtils.copy(stream, System.out)
    }
  }

  /**
   * Record the registered tables for fesql to run.
   *
   * @param name
   * @param df
   */
  def registerTable(name: String, df: DataFrame): Unit = {
    registeredTables.put(name, df)
  }

  /**
   * Return the string of Spark session.
   *
   * @return
   */
  override def toString: String = {
    sparkSession.toString()
  }

  /**
   * Stop the Spark session.
   */
  def stop(): Unit = {
    sparkSession.stop()
  }

}
