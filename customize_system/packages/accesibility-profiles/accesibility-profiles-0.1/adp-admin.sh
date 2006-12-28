#!/bin/bash

function print_help
{
    echo "Uso: adp-admin USUARIO PERFIL [OPCION]"
    echo "  OPCION: -f (aplicar sin confirmar)"
}

function apply_profile
{
    su $1 -c "unzip -o $3 -d $2"

    if [ -d $2/.gconf.xml.defaults ]; then
	cp -r $2/.gconf.xml.defaults/* $2/.gconf/
    fi
    if [ -e $2/metadata ]; then
	rm -f $2/metadata
    fi

    exit 0
}

if [ $# -lt 2 ]; then
    print_help
    exit -1
fi

if [ -d /home/$1 ]; then
    USER_HOME=/home/$1
    USER_NAME=$1
else
    echo "Usuario \"$1\" no existe"
    exit -1
fi

if [ -e "/etc/desktop-profiles/$2.zip" ]; then
    PROFILE_PATH=/etc/desktop-profiles/$2.zip
else
    echo "El perfil \"$2\" no existe"
    exit -1
fi

if [ $3 ]; then
    case $3 in
	"-f" ) apply_profile ${USERNAME} ${USER_HOME} ${PROFILE_PATH} && exit 0 ;;
	* ) echo "Opción $3 no reconocida" && exit -1 ;;
    esac
else
    echo "Se va a aplicar el perfil: \"$2\" al usuario: \"$1\""
    echo "Desea realizar la aplicación (s/n)"
    read OPT
    while [ true ]; do
	case $OPT in
	    s ) apply_profile ${USER_NAME} ${USER_HOME} ${PROFILE_PATH} && exit 0 ;;
	    n ) echo "Aplicación cancelada" && exit 0 ;;
	    * ) echo "Pulse \"s\" para aplicar o \"n\" para cancelar" ;;
	esac
	read OPT
    done
fi