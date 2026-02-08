#!/bin/sh

cd $1/Core/Src

errorHandler=`cat main.c | grep "void Error_Handler(void)" | grep -v grep`

if [ -n "$errorHandler" ]
then
echo "Removing old error Handler method"

sed -i '/.*void Error_Handler(void).*/,+9d' main.c

#sed -i '/USER CODE END 4/{n;N;N;N;N;d;}' main.c
else
echo "Old errorHandler already removed"
fi
