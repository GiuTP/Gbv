<a id="readme-top"></a>

[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![MIT License][license-shield]][license-url]
[![LinkedIn][linkedin-shield]][linkedin-url]

<br />
<div align="center">
  <h3 align="center">📦 Gbv</h3>

  <p align="center">
    Agrupador de arquivos binários sem compactação — trabalho da disciplina Programação 2 (CI1002) na UFPR.
    <br />
    <a href="https://github.com/GiuTP/Gbv/issues/new?labels=bug">Reportar Bug</a>
    &middot;
    <a href="https://github.com/GiuTP/Gbv/issues/new?labels=enhancement">Sugerir Melhoria</a>
  </p>
</div>

---

<!-- SUMÁRIO -->
<details>
  <summary>Sumário</summary>
  <ol>
    <li><a href="#-sobre-o-projeto">Sobre o Projeto</a>
      <ul>
        <li><a href="#-construído-com">Construído com</a></li>
      </ul>
    </li>
    <li><a href="#-fundamentação--arquitetura">Fundamentação / Arquitetura</a></li>
    <li><a href="#-entidades--componentes">Entidades / Componentes</a></li>
    <li><a href="#-dinâmica-e-fluxo-de-execução">Dinâmica e Fluxo de Execução</a></li>
    <li><a href="#-estruturas-de-dados-tads">Estruturas de Dados (TADs)</a></li>
    <li><a href="#-estrutura-do-projeto">Estrutura do Projeto</a></li>
    <li>
      <a href="#-instalação">Instalação</a>
      <ul>
        <li><a href="#-pré-requisitos">Pré-requisitos</a></li>
        <li><a href="#-compilação">Compilação</a></li>
        <li><a href="#-comandos-úteis">Comandos Úteis</a></li>
      </ul>
    </li>
    <li><a href="#-dificuldades-e-aprendizados">Dificuldades e Aprendizados</a></li>
    <li><a href="#-licença">Licença</a></li>
    <li><a href="#-contato">Contato</a></li>
    <li><a href="#-agradecimentos">Agradecimentos</a></li>
  </ol>
</details>

---

## 📖 Sobre o Projeto

**Gbv** é um agrupador de arquivos binários implementado em linguagem C, desenvolvido para a disciplina **Programação 2 (CI1002)** da **Universidade Federal do Paraná (UFPR)**.

O programa permite reunir múltiplos arquivos em um único contêiner `.gbv`, armazenando cada arquivo junto de seus metadados (nome, tamanho, data de inserção e posição). Por se tratar de um agrupador puro — sem compactação — os bytes dos arquivos originais são gravados integralmente, e **não há mecanismo nativo de extração dos arquivos individuais**, que é a forma como o trabalho foi pedido. O objetivo principal foi praticar a leitura e escrita de arquivos binários em C com controle total dos bytes.

<p align="right">(<a href="#readme-top">voltar ao topo</a>)</p>

### 🛠 Construído com

* [![C][C-badge]][C-url]
* [![Linux][Linux-badge]][Linux-url]

<p align="right">(<a href="#readme-top">voltar ao topo</a>)</p>

---

## ⏱ Fundamentação

O arquivo `.gbv` tem um layout binário fixo: um **superbloco** no início guarda o deslocamento (*offset*) onde começam os metadados e o número de documentos armazenados. Os bytes brutos de cada arquivo ficam contíguos logo após o superbloco, e a tabela de metadados (`Document[]`) ocupa o final do contêiner — sendo reescrita a cada operação.

```
+-----------------------------------------------------------+
|              Layout do arquivo .gbv                       |
|                                                           |
|  [SBlock] offset=X, count=N                               |
|  [bytes do doc 1] [bytes do doc 2] ... [bytes do doc N]   |
|  [Document[0]] [Document[1]] ... [Document[N-1]]          |
+-----------------------------------------------------------+
```

Operações como `add` e `remove` realizam **deslocamento físico dos bytes** no arquivo para abrir ou fechar espaço, mantendo os dados consistentes sem fragmentação.

<p align="right">(<a href="#readme-top">voltar ao topo</a>)</p>

---

## 👥 Componentes

| Entidade | Atributos Principais | Descrição |
|---|---|---|
| **`SBlock`** (superbloco) | `offset`, `count` | Cabeçalho do `.gbv`; registra onde começam os metadados e quantos documentos existem. Reescrito a cada operação. |
| **`Document`** | `name[256]`, `size`, `date`, `offset` | Metadados de um arquivo agrupado: nome, tamanho em bytes, data de inserção e posição dos seus dados no contêiner. |
| **`Library`** | `docs` (vetor dinâmico), `count` | Diretório em memória — vetor de `Document` carregado na abertura da biblioteca e descarregado ao término. |

<p align="right">(<a href="#readme-top">voltar ao topo</a>)</p>

---

## 🔄 Dinâmica e Fluxo de Execução

| Comando | Comportamento |
|---|---|
| `gbv -a <lib> <doc...>` | Abre a biblioteca (criando se não existir), e para cada documento: se for novo, acrescenta seus bytes ao final dos dados e registra seus metadados; se já existir com mesmo nome, substitui no lugar — deslocando bytes vizinhos caso os tamanhos difiram. |
| `gbv -r <lib> <doc>` | Localiza o documento na tabela, move fisicamente os bytes dos documentos posteriores para "fechar" o buraco deixado e reduz o tamanho do contêiner com `ftruncate`. |
| `gbv -l <lib>` | Lê os metadados já carregados em memória e imprime nome, tamanho, offset e data de cada documento armazenado. |
| `gbv -v <lib> <doc>` | Localiza o documento, abre o `.gbv` e exibe seus bytes em hexadecimal em blocos de 512 bytes; aceita `n` (próximo), `p` (anterior) e `q` (sair) para navegação interativa. |

<p align="right">(<a href="#readme-top">voltar ao topo</a>)</p>

---

## 🧩 Estruturas de Dados (TADs)

O projeto exercita a construção e o uso de estruturas de dados voltadas a arquivos:

* **Superbloco (`SBlock`):** estrutura de cabeçalho de tamanho fixo, sempre na posição 0 do arquivo. Atua como ponteiro para a tabela de metadados e como contador de documentos, permitindo localizar e carregar o diretório com uma única leitura posicionada.
* **Vetor dinâmico de `Document`:** alocado com `malloc`/`realloc` em memória conforme documentos são adicionados ou removidos. Cada entrada mapeia um arquivo agrupado para sua posição física no contêiner, funcionando como índice em memória de um sistema de arquivos simples.

<p align="right">(<a href="#readme-top">voltar ao topo</a>)</p>

---

## 📁 Estrutura do Projeto

```
Gbv/
├── bin/                 executável final gerado pelo make (gbv)
├── build/               arquivos-objeto intermediários (*.o)
├── include/             interfaces e definições dos módulos (*.h)
│   ├── gbv.h
│   └── util.h
├── src/                 código-fonte em C (*.c)
│   ├── gbv.c
│   ├── util.c
│   └── main.c
├── compile_flags.txt    configuração de flags para o LSP clangd
├── Makefile             automação de compilação e limpeza
└── README.md
```

<p align="right">(<a href="#readme-top">voltar ao topo</a>)</p>

---

## 🚀 Instalação

### 📦 Pré-requisitos

É necessário dispor de um compilador C com suporte a C99 e do GNU Make. No Ubuntu/Debian:

```sh
sudo apt update
sudo apt install build-essential valgrind -y
```

### 🔧 Compilação

1. Clone o repositório:
   ```sh
   git clone https://github.com/GiuTP/Gbv.git
   cd Gbv
   ```

2. Compile o projeto:
   ```sh
   make
   ```
   O executável será gerado em `bin/gbv`.

3. Execute o programa:
   ```sh
   ./bin/gbv -l minha_biblioteca.gbv
   ```

### ⚙ Comandos Úteis

| Comando | Descrição |
|---|---|
| `make` | Compila os arquivos fonte e gera `bin/gbv` |
| `make valgrind` | Executa o programa sob o Valgrind para checagem de vazamentos de memória |
| `make clean` | Remove os arquivos-objeto e o executável |

<p align="right">(<a href="#readme-top">voltar ao topo</a>)</p>

---

## 📚 Dificuldades e Aprendizados

Principais desafios enfrentados durante o desenvolvimento:

- **Como o C lida com leitura e escrita de arquivos** — No início foi necessário estudar função por função (`fopen`, `fclose`, `fread`, `fwrite`, `fseek`, `ftell`, etc.) consultando fóruns e documentação, para entender o modelo de streams binários antes de começar a implementar qualquer coisa.
- **Como manipular os bytes puros de um arquivo sem corromper dados** — O desafio mais complexo foi a substituição de um arquivo de mesmo nome com tamanho diferente: foi preciso "empurrar" todos os bytes posteriores para abrir espaço (tamanho maior) ou "puxar" para fechar o buraco (tamanho menor), tudo com `fseek`/`fread`/`fwrite` em blocos. Validar a correção usando `hexdump` diretamente nos bytes do `.gbv` também foi parte essencial desse aprendizado.
- **Entender a função de formatação de tempo** — `localtime` e `strftime` foram pesquisadas e testadas para garantir a exibição legível da data de inserção de cada documento.
- **Organização do código em contexto acadêmico** — Decidir o nível de verificação de erros (`fread`/`fwrite`), o uso de comentários e a separação em módulos exigiu equilíbrio entre legibilidade e completude do código entregue.

<p align="right">(<a href="#readme-top">voltar ao topo</a>)</p>

---

## 📄 Licença

O código-fonte deste projeto está distribuído sob a licença **MIT**. Consulte o arquivo `LICENSE` para mais informações.

<p align="right">(<a href="#readme-top">voltar ao topo</a>)</p>

---

## 📬 Contato

GiuTP — [github.com/GiuTP](https://github.com/GiuTP)

E-Mail — giulianotpt@gmail.com

Link do projeto: [https://github.com/GiuTP/Gbv](https://github.com/GiuTP/Gbv)

<p align="right">(<a href="#readme-top">voltar ao topo</a>)</p>

---

## 🙏 Agradecimentos

* [Prof. Jorge Pires Correia (DINF/UFPR)](https://secret.pages.c3sl.ufpr.br/author/jorge-pires-correia/) — pela especificação do trabalho que motivou o estudo de I/O binário em C
* [Best-README-Template](https://github.com/othneildrew/Best-README-Template) — template base deste README

<p align="right">(<a href="#readme-top">voltar ao topo</a>)</p>

---

<!-- MARKDOWN LINKS & IMAGES -->
[stars-shield]: https://img.shields.io/github/stars/GiuTP/Gbv.svg?style=for-the-badge
[stars-url]: https://github.com/GiuTP/Gbv/stargazers
[issues-shield]: https://img.shields.io/github/issues/GiuTP/Gbv.svg?style=for-the-badge
[issues-url]: https://github.com/GiuTP/Gbv/issues
[license-shield]: https://img.shields.io/github/license/GiuTP/Gbv.svg?style=for-the-badge
[license-url]: https://github.com/GiuTP/Gbv/blob/main/LICENSE
[linkedin-shield]: https://img.shields.io/badge/-LinkedIn-black.svg?style=for-the-badge&logo=linkedin&colorB=555
[linkedin-url]: https://www.linkedin.com/in/giuliano-tavares/
[C-badge]: https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white
[C-url]: https://en.wikipedia.org/wiki/C_(programming_language)
[Linux-badge]: https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black
[Linux-url]: https://www.kernel.org/
