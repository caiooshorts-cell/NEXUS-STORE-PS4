# NEXUS STORE — cliente nativo PS4

Esta pasta inicia a etapa do cliente homebrew para PS4. O painel web e o catálogo `catalog.json` continuam na raiz e no GitHub Pages.

## Objetivo do cliente
- Ler o catálogo remoto publicado no GitHub Pages.
- Exibir jogos e detalhes em português.
- Navegar com o DualShock 4.
- Baixar PKGs diretamente da URL externa para o armazenamento interno.
- Mostrar progresso e velocidade; retomar somente quando o servidor aceitar HTTP Range.
- Validar tamanho e SHA-256 quando o catálogo fornecer esses dados.

## Estado
Ainda não há um PKG instalável nesta etapa. O próximo commit adicionará o código-fonte e o workflow OpenOrbis; o artefato só será considerado pronto depois de uma compilação bem-sucedida no GitHub Actions e teste no console.

## Segurança
Use somente arquivos homebrew e conteúdos que você tenha autorização para distribuir. Não inclua tokens, senhas ou URLs privadas no repositório.
