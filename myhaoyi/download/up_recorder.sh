#!/bin/bash

# define rpm name...
mySrcDir="htdocs"
mySrcName="recorder"
mySrcFile=$mySrcName"-0.0.1-1.x86_64.rpm"
mySrcUrl="https://myhaoyi.com/download/"$mySrcFile

# download newest rpm package...
#rm -rf ./$mySrcFile
wget -c --limit-rate=300k $mySrcUrl

# uninstall web...
myResult=`rpm -qa | grep $mySrcName`
if [ ! -z $myResult ]; then
  echo "=== uninstall $myResult ==="
  rpm -e $myResult
fi

# rename web directory...
myDate=$(date +%Y-%m-%d)
mySrcPath="/weike/"$mySrcDir
myDesPath=$mySrcPath"-"$myDate
if [ -d $mySrcPath ]; then
  echo "=== rename directory ==="
  mv $mySrcPath $myDesPath
fi

# install web...
rpm -ivh $mySrcFile

# rename rpm file => if exist...
myDesFile=$myDate"-"$mySrcFile
if [ -f $mySrcFile ]; then
  echo "=== rename rpm file ==="
  mv $mySrcFile $myDesFile
fi
