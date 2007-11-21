#! /bin/bash
#
# @author: Valentin Al. Sitnick, Petr Ivanov
# @version: $Revision$
#

# STEP 1: set the deafult values before argument parsing
workmode="run_only"
testmode="all"
testname=""

function die(){
    echo $1
    exit 1
}

# STEP 2: parse the argument
if [ "$1" = "" ]; then
    #start only for all JVMTI tests
    echo "start only for all JVMTI tests"
else
    if [ "$1" = "-o" ]; then
        shift;
        case $1 in
            "b"  ) workmode="build_only";;
            "r"  ) workmode="run_only";;
            "br" ) workmode="build_and_run";;
            *    ) die "Incorrect options mode. Use <-help> for details" ;;
        esac
    shift;
    fi

    case $1 in
        "-sin" ) testmode="single";
            shift;
            while [ "$1" != "" ];
            do
                testname=$testname"$1"" ";
                shift;
            done;;
        "-grp" ) testmode="group";  testname="$2";;
        "-all" ) testmode="all";    testname="$2";;
        ""     ) testmode="all";    testname="$2";;
        *      ) die "Incorrect start mode. Use <-help> for details" ;;
    esac
fi

echo
echo "What we will do?  -----------------  $workmode"
echo "What test mode do you select ?  ---  $testmode"
echo "What test do you select -----------  $testname"
echo "Default compiler is ---------------  $C_COMPILER"
echo

# STEP 3: forming of test list according to testmode
case $testmode in
    "single" ) ;;
    "group"  ) source ./test_list.sh;
        for index in $(seq 1 21) ; do
            if [ "${testname}" = "${group_name[${index}]}" ] ; then
                testname="${group[${index}]}";
            fi
        done;;
    "all"    ) source ./test_list.sh;
        for index in $(seq 1 21) ; do
            testname=$testname" ${group[${index}]}";
        done ;;
esac

# STEP 4: Continue of work according to testmode
# Next step is 5 (but if you do not want to build test(s) (run only)
# next step is 7 )
if [ $workmode = "run_only" ]; then
    source ./test_run.sh
else
    source ./test_build.sh

    if [ $workmode = "build_and_run" ]; then
        source ./test_run.sh
    fi
fi

# vim:ff=unix

