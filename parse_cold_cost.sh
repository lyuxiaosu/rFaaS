#!/bin/bash

function usage {
        echo "$0 [output log file] [cost log file]"
        exit 1
}

if [ $# != 2 ] ; then
        usage
        exit 1;
fi

log1=$1
log2=$2
awk '/avg/ {sum+=$11; count++} END {print "Average avg value: ", sum/count}' $log1


awk -F, '{
  if ($1 ~ /^[0-9]+$/) {  # 检查第一列是否是数字，排除文字行
    sum = 0;
    for (i = 2; i <= NF; i++) sum += $i;  # 计算当前行的总和
    total += sum;
    last_column_total += $NF;  # 累加最后一列的值
    count++;
  }
} END {
  if (count > 0) {
    print "Average of all columns:", total / count;
    print "Average of the last column:", last_column_total / count;
  } else print "No valid data found.";
}' $log2

