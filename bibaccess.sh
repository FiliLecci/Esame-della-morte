#!/bin/bash

if [ "$1" = "--loan" ]; then
	modalita="LOAN"
elif [ "$1" = "--query" ]; then
	modalita="QUERY"
else
	echo "Modalità non valida"
	exit 1
fi

sommaTot=0

for file in logs/*.log; do
	sommaFile=0
	# la seconda condizione serve perché l'ultima riga, se non seguita da \n, non viene riconosciuta
	# line dopo la lettura contiene sempre il valore, la condizione controlla se non è vuota e nel caso va avanti
	while read -r line || [ -n "$line" ];
	do
		if [[ "$line" == "$modalita"* ]]; then
			array=( $line )
			numero=`echo ${array[1]} |  tr -dc '[:alnum:]' | tr '[:upper:]' '[:lower:]'`
			sommaFile=`expr $sommaFile + $numero`
		fi
	done < "$file"
	echo "$file $sommaFile"
	sommaTot=`expr $sommaTot + $sommaFile`
done
echo $modalita $sommaTot