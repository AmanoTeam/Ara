# SparkleC

SparkleC é um programa escrito em C que usufruí das APIs da plataforma de cursos Hotmart para possibilitar que usuários baixem cursos que tenham previamente comprado diretamente para o armazenamento local de seu computador ou smartphone.

# Instalação

O programa está disponível para Android, Windows, Linux e MacOS.

Note que, embora versões para MacOS estejam disponíveis, as mesmas não foram testadas. Caso encontre (ou não) problemas ao executar o SparkleC nessa plataforma, [reporte-nos](https://github.com/Kartatz/SparkleC/issues).

A ferramenta depende do [ffmpeg](https://ffmpeg.org/download.html) para decodificar e desencriptar os arquivos de vídeo baixados. Ele não funcionará sem ela, portanto instale-o em sua máquina antes de tudo.

## Instalando no Windows

Note que o programa precisa de uma versão igual ou superior ao Windows 7 para funcionar adequadamente.

Abaixo estão os links para download do SparkleC para diversas arquiteturas. Você precisa baixar a versão que [corresponde a arquitetura do seu processador](https://support.microsoft.com/pt-br/windows/versões-de-32-bits-e-64-bits-do-windows-perguntas-frequentes-c6ca9541-8dce-4d48-0415-94a3faa2e13d), do contrário poderá acabar enfrentando problemas para executar o programa.

A grande maioria dos computadores atuais são fabricados com processadores `x86_64`.

- [SparkleC para Windows x86_64](https://github.com/Kartatz/SparkleC/releases/download/v0.2/x86_64-w64-mingw32.zip)
- [SparkleC para Windows x86](https://github.com/Kartatz/SparkleC/releases/download/v0.2/i686-w64-mingw32.zip)
- [SparkleC para Windows arm](https://github.com/Kartatz/SparkleC/releases/download/v0.2/armv7-w64-mingw32.zip)
- [SparkleC para Windows arm64](https://github.com/Kartatz/SparkleC/releases/download/v0.2/aarch64-w64-mingw32.zip)

Após baixar o arquivo compactado correspondente, extraía-io usando o WinRAR. Após isso, navegue através das pastas que foram criadas até encontrar um arquivo nomeado `sparklec.exe`.

Dê um duplo clique nele. Se tudo correu bem, isso fará com que uma janela do prompt de comando abra. Note que não é necessário executar o programa como administrador.

## Instalando no Linux e MacOS

Versões para Linux:

- [SparkleC para Linux x86_64](https://github.com/Kartatz/SparkleC/releases/download/v0.2/x86_64-linux-gnu.tar.xz)
- [SparkleC para Linux x86](https://github.com/Kartatz/SparkleC/releases/download/v0.2/i686-linux-gnu.tar.xz)
- [SparkleC para Linux arm](https://github.com/Kartatz/SparkleC/releases/download/v0.2/arm-linux-gnueabi.tar.xz)
- [SparkleC para Linux arm64](https://github.com/Kartatz/SparkleC/releases/download/v0.2/aarch64-linux-gnu.tar.xz)

Versões para MacOS:

- [SparkleC para MacOS X x86_64](https://github.com/Kartatz/SparkleC/releases/download/v0.2/x86_64-apple-darwin.tar.xz)
- [SparkleC para MacOS X x86](https://github.com/Kartatz/SparkleC/releases/download/v0.2/i386-apple-darwin.tar.xz)

Assumo você já sabe como usar o SparkleC nessas plataformas.

## Instalando no Android

O programa em si funciona no Android 5 ou superior, mas para seguir as instruções abaixo você precisará de no mínimo um smartphone com Android 7.

Baixe o Termux através do F-Droid e instale-o. Você pode obtê-lo [clicando aqui](https://f-droid.org/repo/com.termux_118.apk). Após a instalação, abra-o e insira as seguintes linhas de comando no terminal:

```bash
apt update --assume-yes
apt install --assume-yes wget
bash <<< "$(wget 'https://raw.githubusercontent.com/Kartatz/SparkleC/master/scripts/android-install.sh' --output-document=-)"

```

Se a instalação correu bem, você verá essa mensagem após a conclusão:

```
Instalação concluída! Execute o comando "sparklec" sempre que quiser usar a ferramenta.
```

## Compilando

Você pode compilar o SparkleC localmente em sua máquina. Basta ter um compilador C compatível com a especificação do C99 e o CMake, versão 3.13 ou superior.

O projeto atualmente usa características específicas que não fazem parte da especificação oficial do C, as quais só não reconhecidas pelos compiladores Clang e GCC. Você pode vir a encontrar erros ao tentar compilar o código em outros compiladores fora os que foram mencionados. Isso é intencional e eu não pretendo adicionar suporte a qualquer outro.

O SparkleC inclui todas as suas dependências como submódulos no git. Para clonar o repositório junto com todas as suas dependências, você precisa específicar a flag `--resursive`:

```bash
git clone --recursive 'https://github.com/Kartatz/SparkleC.git'
```

Execute os comandos abaixo para compilar o projeto:

```bash
mkdir SparkleC/build && cd SparkleC/build
cmake ../
cmake --build ./
```

Após a compilação, você pode executar o SparkleC através do diretório no qual você já está ou realizar a instalação usando o comando abaixo:

```bash
cmake --install ./
```