#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2021 4Paradigm
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# -*- coding: utf-8 -*-
from framework.test_suite import TestSuite
from job_helper import JobHelper, RtidbClient

class CreateTable(TestSuite):
    """
    create_table 操作
    """
    def setUp(self):
        """

        :return:
        """
        pass

    def tearDown(self):
        """

        :return:
        """
        pass

    def testCreateTableNameDuplicate(self):
        """

        已经存在的table名，再次执行create操作失败
        """
        jobHelper = JobHelper()
        jobHelper.append(jobHelper.rtidbClient.create_table, name='table_duplacate')
        jobHelper.append(jobHelper.rtidbClient.create_table, name='table_duplacate')
        retStatus = jobHelper.run(failonerror=False)
        self.assertFalse(retStatus)

    def testCreateTableNameEmpty(self):
        """

        table名为空
        """
        jobHelper = JobHelper()
        jobHelper.append(jobHelper.rtidbClient.create_table, name='')
        retStatus = jobHelper.run(failonerror=False)
        self.assertFalse(retStatus)

    def testCreateTableNameIllegal(self):
        """

        table名有特殊字符
        """
        jobHelper = JobHelper()
        jobHelper.append(jobHelper.rtidbClient.create_table, name='ill_*/.&@#')
        retStatus = jobHelper.run(failonerror=False)
        self.assertTrue(retStatus)

    def testCreateTableTableIdDuplicate(self):
        """

        已经存在的table_id，再次执行create操作失败
        """
        jobHelper = JobHelper()
        jobHelper.append(jobHelper.rtidbClient.create_table, tid='123456')
        jobHelper.append(jobHelper.rtidbClient.create_table, tid='123456')
        retStatus = jobHelper.run(failonerror=False)
        self.assertFalse(retStatus)

    def testCreateTableTableIdEmpty(self):
        """

        table_id配空
        """
        jobHelper = JobHelper()
        jobHelper.append(jobHelper.rtidbClient.create_table, tid=0)
        retStatus = jobHelper.run(failonerror=False)
        self.assertFalse(retStatus)

    def testCreateTablePidEmpty(self):
        """

        pid配空
        """
        jobHelper = JobHelper()
        jobHelper.append(jobHelper.rtidbClient.create_table, pid=1)
        retStatus = jobHelper.run(failonerror=False)
        self.assertTrue(retStatus)

    def testCreateTablePid0(self):
        """

        pid=0
        """
        jobHelper = JobHelper()
        jobHelper.append(jobHelper.rtidbClient.create_table, pid=0)
        jobHelper.append(jobHelper.rtidbClient.put)
        jobHelper.append(jobHelper.rtidbClient.scan)
        retStatus = jobHelper.run()
        self.assertTrue(retStatus)

    def testCreateTablePidMax(self):
        """

        pid=max
        """
        jobHelper = JobHelper()
        import sys
        jobHelper.append(jobHelper.rtidbClient.create_table, pid=sys.maxint)
        jobHelper.append(jobHelper.rtidbClient.put, pid=sys.maxint)
        jobHelper.append(jobHelper.rtidbClient.scan, pid=sys.maxint)
        retStatus = jobHelper.run()
        self.assertTrue(retStatus)

    def testCreateTableSameToken(self):
        """

        table名，table_id，partion_id，segment_count配相同数字
        """
        jobHelper = JobHelper()
        import sys
        jobHelper.append(jobHelper.rtidbClient.create_table,
                         name='123',
                         tid=123,
                         pid=123)
        jobHelper.append(jobHelper.rtidbClient.put,
                         tid=123,
                         pid=123)
        jobHelper.append(jobHelper.rtidbClient.scan,
                         tid=123,
                         pid=123)
        retStatus = jobHelper.run()
        self.assertTrue(retStatus)
