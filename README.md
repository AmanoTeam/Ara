# Ara

O Ara é um programa escrito em C que usufruí das APIs de algumas plataformas que distribuem material didático para possibilitar que usuários baixem conteúdos que tenham previamente adquirido diretamente para o armazenamento local de seu computador ou smartphone.

# Instalação

O programa está disponível para Android, Windows, Linux, MacOS, Haiku, SerenityOS e sistemas BSD.

## Instalando no Windows

Note que o programa precisa de uma versão igual ou superior ao Windows 7 para funcionar adequadamente.

Abaixo estão os links para download do Ara para diversas arquiteturas. Você precisa baixar a versão que [corresponde a arquitetura do seu processador](https://support.microsoft.com/pt-br/windows/versões-de-32-bits-e-64-bits-do-windows-perguntas-frequentes-c6ca9541-8dce-4d48-0415-94a3faa2e13d), do contrário poderá acabar enfrentando problemas para executar o programa.

A grande maioria dos computadores atuais são fabricados com processadores `x86_64`.

- [Ara para Windows x86_64](https://github.com/AmanoTeam/Ara/releases/latest/download/x86_64-w64-mingw32.zip)
- [Ara para Windows x86](https://github.com/AmanoTeam/Ara/releases/latest/download/i686-w64-mingw32.zip)
- [Ara para Windows arm](https://github.com/AmanoTeam/Ara/releases/latest/download/armv7-w64-mingw32.zip)
- [Ara para Windows arm64](https://github.com/AmanoTeam/Ara/releases/latest/download/aarch64-w64-mingw32.zip)

Após baixar o arquivo compactado correspondente, extraía-io usando o WinRAR. Após isso, navegue através das pastas que foram criadas até encontrar um arquivo nomeado `ara.exe`.

Dê um duplo clique nele. Se tudo correu bem, isso fará com que uma janela do prompt de comando abra. Note que não é necessário executar o programa como administrador.

## Instalando em outros sistemas

Na [página de lançamentos](https://github.com/AmanoTeam/Ara/releases) do projeto existem binários compatíveis com diversas arquiteturas e sistemas. Navegue entre eles e baixe aquele que corresponde com as especificações da sua máquina.

O modo de uso é basicamente o mesmo em todos os sistemas:

1. Baixe o arquivo compactado `.tar.xz` ou `.zip` correspondente
2. Extraia todos os arquivos e diretórios para algum local do seu disco
3. Abra o seu terminal ou prompt de comando e execute o binário localizado em `bin/ara` (No caso do Windows, é `bin\ara.exe`)

O Ara foi desenvolvido levando a portabilidade em consideração. Graças a isso você não precisa instalar os arquivos extraídos em nenhum local específico do sistema de arquivos da sua máquina para que possa executá-lo.

# Compilando

Você pode compilar o Ara localmente em sua máquina. Basta ter um compilador C compatível com a especificação do C99 e o CMake, versão 3.13 ou superior.

O projeto atualmente usa características específicas que não fazem parte da especificação oficial do C, as quais só não reconhecidas pelos compiladores Clang e GCC. Você pode vir a encontrar erros ao tentar compilar o código em outros compiladores fora os que foram mencionados. Isso é intencional e eu não pretendo adicionar suporte a qualquer outro.

O Ara inclui todas as suas dependências como submódulos no git. Para clonar o repositório junto com todas as dependências necessárias para compilá-lo, você precisa específicar a opção `--recursive`:

```bash
git clone --depth=1 --recursive https://github.com/AmanoTeam/Ara.git
```

Navegue até a raiz do diretório onde o código fonte está e execute os comandos abaixo para compilar o projeto:

```bash
cmake -DCMAKE_BUILD_TYPE=MinSizeRel .
cmake --build .
```

Após a compilação, você pode realizar a instalação dos arquivos gerados utilizando o comando abaixo:

```bash
cmake --install ./
```

# Problemas

Reporte qualquer tipo de problema que esteja enfrentando através das [issues](https://github.com/AmanoTeam/Ara/issues) no GitHub.

Ao fazer isso, sempre forneça o maior número possível de informações que você puder, de forma clara e objetiva.

# Notas

## Seu direito

Usar o que você comprou é um direito seu. Já **como** ou **onde** você planeja usá-lo é algo que fica a seu total critério e também uma coisa para a qual eu particularmente não dou a mínima.

Toda e qualquer possível responsabilidade decorrentes do uso do programa ou do conteúdo por ele baixado recaem exclusivamente sobre quem está usando-o.

## Minhas obrigações

Eu não tenho nenhuma – não com você. Assim como todo programa gratuito, o Ara não possui nenhuma obrigação de corresponder as espectativas de quem o usa.

Caso encontre um erro no programa, me conte e eu tentarei resolvê-lo. Caso possua alguma sugestão, me fale e eu tentarei ser o mais cordial possível enquanto avalio se ela realmente é útil e se vale a pena investir o meu tempo nisso.

Não reclame, faça exigências ou falte ao respeito com nenhum dos mantenedores do projeto e seus contribuidores.

## Malware? Trojan?

Softwares de antivírus para computadores – mais especificamente aqueles disponíveis para Windows – são frequentemente usados para analisar e detectar executáveis potencialmente infectados com malware. Infelizmente nenhuma dessas ferramentas são 100% eficazes em suas detecções e ocasionalmente acusam programas legítimos de serem um executável malicioso. Chamamos essas acusações errôneas de falsos-positivos.

O Ara é frequentemente acusado por softwares de antivírus, mas ele não é um programa malicioso. Caso esteja recebendo um aviso do seu antivírus sobre algo de errado com o Ara, você pode ignorá-lo sem preocupações.

_Caso esteja no Windows, pode ser que seja necessário desativar o Windows Defender para que seja possível executar o binário do Ara._
