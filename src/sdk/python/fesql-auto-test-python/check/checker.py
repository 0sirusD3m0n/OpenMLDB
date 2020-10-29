#! /usr/bin/env python
# -*- coding: utf-8 -*-

from fedb.sql_router_sdk import DataTypeName
from abc import ABCMeta,abstractmethod
from nb_log import LogManager
import re
import datetime
from entity.fesql_result import FesqlResult
import util.fesql_util as fesql_util
import check.fesql_assert as fesql_assert


log = LogManager('fesql-auto-test').get_logger_and_add_handlers()
class Checker(metaclass=ABCMeta):
    @abstractmethod
    def check(self):
        pass

class BaseChecker(Checker):

    def __init__(self,fesqlCase,fesqlResult:FesqlResult):
        self.fesqlCase = fesqlCase
        self.fesqlResult = fesqlResult

class RowsChecker(BaseChecker):
    def __init__(self,fesqlCase,fesqlResult):
        super(RowsChecker,self).__init__(fesqlCase,fesqlResult)

    def check(self):
        log.info("result check")
        expect = self.fesqlCase['expect'].get('rows')
        # self.toStringByNone(expect)
        actual = self.fesqlResult.result
        log.info("expect:{}".format(expect))
        log.info("actual:{}".format(actual))
        assert len(expect)==len(actual)
        columns = self.fesqlCase['expect'].get('columns')
        if columns!=None and len(columns)>0 :
            expect = self.convertRows(expect,columns)
            log.info("convert expect:{}".format(expect))
        orderName = self.fesqlCase['expect'].get('order')
        if orderName!=None and len(orderName)>0:
            if hasattr(self.fesqlResult.rs, "_cursor_description"):
                desc = self.fesqlResult.rs._cursor_description()
                index = fesql_util.getIndexByColumnName(desc ,orderName)
            else:
                schema = self.fesqlResult.resultSchema
                index = fesql_util.getIndexByColumnName(schema,orderName)
            log.info("old data: {}".format(actual))
            log.info("old data: {}".format(expect))
            expect = sorted(expect,key= lambda x:x[index])
            actual = sorted(actual, key=lambda x: x[index])
            log.info("order expect:{}".format(expect))
            log.info("order actual:{}".format(actual))
        fesql_assert.check_rows(actual,expect)

    def toStringByNone(self,rows:list):
        for row in rows:
            for i in range(len(row)):
                if row[i]==None:
                    row[i] = 'None'

    def convertRows(self,rows:list,columns:list):
        list = []
        for row in rows:
            list.append(self.convertList(row,columns))
        return list

    def convertList(self,datas:list,columns:list):
        list = []
        for index,value in enumerate(datas):
            if value == None :
                list.append(None)
            else:
                data = str(value);
                column = columns[index]
                log.info("column is:{}".format(column))
                list.append(self.convertData(data,column))
        return list

    def convertData(self,data:str,column:str):
        ss = re.split("\\s+",column)
        type = ss[-1]
        obj = None
        if data == 'None':
            return 'None'
        if type == 'int' or type == 'int32':
            obj = int(data)
        elif type == 'int64':
            obj = int(data)
        elif type == 'bigint':
            obj = int(data)
        elif type == 'smallint' or type == 'int16':
            obj = int(data)
        elif type == 'float':
            obj = float(data)
        elif type == 'double':
            obj = float(data)
        elif type == 'bool':
            obj = eval(data)
        elif type == 'string':
            obj = str(data)
        elif type == 'timestamp':
            obj = int(data)
        elif type == 'date':
            obj = data
        else:
            obj = data
        return obj

class ColumnsChecker(BaseChecker):
    def __init__(self,fesqlCase,fesqlResult):
        super(ColumnsChecker,self).__init__(fesqlCase,fesqlResult)

    def check(self):
        log.info("columns check")
        """
        expect = self.fesqlCase['expect'].get('columns')
        fesql_util.convertExpectTypes(expect)
        schema = self.fesqlResult.resultSchema
        assert schema.GetColumnCnt() == len(expect),"actual:"+str(schema.GetColumnCnt())+",expect:"+str(len(expect))
        for index,value in enumerate(expect):
            actual = schema.GetColumnName(index)+" "+str(fesql_util.getColumnType(DataTypeName(schema.GetColumnType(index))))
            assert actual == value,'actual:{},expect:{}'.format(actual,value)
        """

class CountChecker(BaseChecker):
    def __init__(self,fesqlCase,fesqlResult):
        super(CountChecker,self).__init__(fesqlCase,fesqlResult)

    def check(self):
        log.info("count check")
        expect = self.fesqlCase['expect'].get('count')
        actual = self.fesqlResult.count
        assert actual == expect

class SuccessChecker(BaseChecker):
    def __init__(self,fesqlCase,fesqlResult):
        super(SuccessChecker,self).__init__(fesqlCase,fesqlResult)

    def check(self):
        log.info("success check")
        expect = self.fesqlCase['expect'].get('success')
        actual = self.fesqlResult.ok
        assert actual == expect,"actual:{},ecpect:{}".format(actual,expect)

def build(fesqlCase,fesqlResult):
    checkList = []
    if fesqlCase == None :
        return checkList
    checkMap = fesqlCase['expect']
    for key in checkMap:
        if key=='rows' :
            checkList.append(RowsChecker(fesqlCase,fesqlResult))
        elif key == 'success':
            checkList.append(SuccessChecker(fesqlCase,fesqlResult))
        elif key == 'count':
            checkList.append(CountChecker(fesqlCase,fesqlResult))
        elif key == 'columns':
            checkList.append(ColumnsChecker(fesqlCase,fesqlResult))
        elif key == 'order':
            pass
        else:
            assert False, "No Checker Available:"+key
    return checkList
