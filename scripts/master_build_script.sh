#!/bin/sh

cd $1/scripts

pwd

./printf_redirect_code.sh $1

./mod_code_template.sh $1

./add_error_handler.sh $1