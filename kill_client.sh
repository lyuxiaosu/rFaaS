#!/bin/bash

pid=`ps -ef|grep  "multi_functions"|grep -v grep |awk '{print $2}'`
echo $pid
sudo kill -9 $pid

