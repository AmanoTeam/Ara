#!/data/data/com.termux/files/usr/bin/bash

set -e

termux-setup-storage <<< 'y'

apt update --assume-yes
apt install --assume-yes ffmpeg tar wget

case "$(dpkg --print-architecture)" in
	aarch64)
		host="aarch64-linux-android";;
	arm)
		host="armv7a-linux-androideabi";;
	amd64)
		host="x86_64-linux-android";;
	x86_64)
		host="x86_64-linux-android";;	
	i*86)
		host="i686-linux-android";;
	x86)
		host="i686-linux-android";;
	*)
		echo "unknown architecture";
		exit 1;;
esac

declare -r url="https://github.com/Kartatz/SparkleC/releases/download/v0.1/${host}.tar.xz"
declare -r filename="${TMPDIR}/binary.tar.xz"

wget "${url}" --output-document="${filename}"

tar --extract --strip='1' --directory="${PREFIX}" --file="${filename}"

mkdir '/sdcard/Download' || true

clear

echo '+ Instalação concluída! Execute o comando "sparklec" sempre que quiser usar a ferramenta.'