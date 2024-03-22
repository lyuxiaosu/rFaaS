#!/bin/bash

pid=`ps -ef|grep  "executor_manager"|grep -v grep |awk '{print $2}'`
echo $pid
sudo kill -9 $pid

