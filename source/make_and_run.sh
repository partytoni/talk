make;
rm -r ../build;
mkdir ../build;
cp *.o ../build;
rm -r ../run;
mkdir ../run ;
cp server ../run ;
cp client ../run;
make clean;
echo "to run type cd ../run and run ./server or ./client or both";
##Shell scripts are run inside a subshell, and each subshell has its own concept of what the current directory is.
## The cd succeeds, but as soon as the subshell exits, you're back in the interactive shell and nothing ever changed there.
