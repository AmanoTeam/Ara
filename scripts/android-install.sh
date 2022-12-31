#!/data/data/com.termux/files/usr/bin/bash

set -u
set -e

declare -r DOWNLOADS_DIRECTORY='/sdcard/Download/SparkleC'

declare -r REPOSITORY='Kartatz/SparkleC'

declare -r INSTALL_PREFIX="${HOME}/.local/opt/SparkleC"

termux-setup-storage <<< 'y' 1>/dev/null

apt install --assume-yes ffmpeg tar jq curl 1>/dev/null

case "$(dpkg --print-architecture)" in
	'aarch64')
		declare -r host='aarch64-linux-android';;
	'arm')
		declare -r host='armv7a-linux-androideabi';;
	'amd64')
		declare -r host='x86_64-linux-android';;
	'x86_64')
		declare -r host='x86_64-linux-android';;
	i*86)
		declare -r host='i686-linux-android';;
	'x86')
		declare -r host='i686-linux-android';;
	*)
		echo 'unknown architecture';
		exit 1;;
esac

declare -r tag_name="$(jq --raw-output '.tag_name' <<< "$(curl  --fail --silent --url "https://api.github.com/repos/${REPOSITORY}/releases/latest")")"

declare -r download_url="https://github.com/${REPOSITORY}/releases/download/${tag_name}/${host}.tar.xz"
declare -r output_file="${TMPDIR}/binary.tar.xz"

curl --fail --silent --location --url "${download_url}" --output "${output_file}"

[ -d "${INSTALL_PREFIX}" ] || mkdir --parent "${INSTALL_PREFIX}"
tar --extract --strip='1' --directory="${INSTALL_PREFIX}" --file="${output_file}"

echo -e '#!/data/data/com.termux/files/usr/bin/bash'"\n\nset -u\nset -e\n\n( [ -d '${DOWNLOADS_DIRECTORY}' ] || mkdir --parent '${DOWNLOADS_DIRECTORY}' ) && cd '${DOWNLOADS_DIRECTORY}'\n\n${INSTALL_PREFIX}/bin/sparklec \${@}" > "${PREFIX}/bin/sparklec"
chmod 700 "${PREFIX}/bin/sparklec"

clear

echo '+ Instalação concluída!'

exit '0'
