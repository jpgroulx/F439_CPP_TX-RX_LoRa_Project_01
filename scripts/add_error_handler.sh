#!/bin/sh

cd $1/Core/Src


newErrorHandler=`cat main.c | grep "void _Error_Handler(const char" | grep -v grep`

if [ -n "$newErrorHandler" ]	# -n returns True if the string length is non-zero
then
	echo "New Error Handler already exists"
else
	echo "Adding new Error Handler"
	sed -i '/USER CODE BEGIN 4/r ../../scripts/error_handler.txt' main.c
fi

cd $1/Core/Inc

prototype=`cat main.h | grep "void Error_Handler\(void\);" | grep -v grep`

if [ -n "$prototype" ]	# -n returns True if the string length is non-zero
then
	echo "Old   Error_Handler prototype not present"
else
	echo "Removing Error_Handler prototype"
	
	sed -i '/void Error_Handler(void);/d' main.h
fi
	
newprototype=`cat main.h | grep "void _Error_Handler(const char" | grep -v grep`

if [ -n "$newprototype" ]	# -n returns True if the string length is non-zero
then
	echo "New _Error_Handler prototype is present"
else
	echo "Adding new _Error_Handler prototype and #define"
	sed -i '/USER CODE BEGIN Private defines/r ../../scripts/error_handler_include.txt' main.h 
fi	