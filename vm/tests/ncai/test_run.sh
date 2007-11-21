#! /bin/bash
#
# @author: Valentin Al. Sitnick, Petr Ivanov
# @version: $Revision$
#

if [ $system = "Linux" ]; then
    export CLASSPATH="$VTSSUITE_ROOT/bin/classes"
#    cp_options="-cp $VTSSUITE_ROOT/bin/classes -Djava.library.path=$VTSSUITE_ROOT/bin/lib"
else
    export CLASSPATH=`cygpath -m $VTSSUITE_ROOT/bin/classes`
#    cp_options="-cp `cygpath -w $VTSSUITE_ROOT/bin/classes` -Djava.library.path=`cygpath -w $VTSSUITE_ROOT/bin/lib`"
fi

echo
echo "Test running..."

#CUSTOM_PROPS=-Djava.bla_bla_bla.property="bla_bla_bla_bla_bla"

for tst in $testname; do

    ${FIND} $VTSSUITE_ROOT/bin/classes -name "${tst}.class" | while read line;
    do
        AGENT="-agentlib:${tst}"
        CLASS="ncai.funcs.${tst}"

        echo $TST_JAVA $cp_options $AGENT $CLASS
        $TST_JAVA $cp_options $AGENT $CLASS

    done
done

