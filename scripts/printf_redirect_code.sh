#!/bin/sh

cd $1/Core/Inc

#redirect=`cat main.h | grep "define REDIRECT_PRINTF" | grep -v grep`

#if [ -n "$redirect" ]	# -n returns True if the string length is non-zero
#then
#	echo "#define REDIRECT_PRINTF already present in main.h"
#else
#	echo "Adding #define REDIRECT_PRINTF to main.h"

#	sed -i '/USER CODE BEGIN Private defines/a #define REDIRECT_PRINTF' main.h
#fi

cd $1/Core/Src

#printf=`cat main.c | grep "ifdef REDIRECT_PRINTF" | grep -v grep`

#if [ -n "$printf" ]		# -n returns True if the string length is non-zero	
#then
#	echo "REDIRECT_PRINTF code already implemented"
#else
#	echo "Adding REDIRECT_PRINTF code"
	
#	sed -i '/USER CODE BEGIN 0/r ../../scripts/printf_redirect_code.txt' main.c
#fi

clearscreen=`cat main.c | grep "Clear the dumb terminal screen" | grep -v grep`

if [ -n "$clearscreen" ]	# -n returns True if the string length is non-zero
then
	echo "Clear Screen already present"
else
	echo "Adding clearn screen printf code"
	sed -i '/USER CODE BEGIN 2/a \\nprintf\("\\x1b[2J\\x1b[H"\);	// Clear the dumb terminal screen' main.c
fi

