# NEXUS STORE — cliente nativo PS4

## Primeira versão nativa
- Interface SDL2 em tela cheia, desenhada para 1920×1080.
- Navegação pelas áreas Início, Jogos, Aplicativos, Emuladores e Ferramentas.
- Entrada por DualShock 4 (direcional, X, O, L1 e R1).
- Teclas do teclado também funcionam como apoio durante testes.
- Tela inicial identificada como demonstração enquanto o catálogo remoto não estiver conectado.

## Próxima etapa
Ligar o cliente ao `catalog.json` publicado no GitHub Pages e implementar busca, detalhes e downloads autorizados com progresso, velocidade, retomada condicionada a HTTP Range e validação SHA-256.

O PKG de preview precisa ser compilado no workflow e depois testado no console real. A compatibilidade com GoldHEN/firmware deve ser confirmada no PS4.

Use somente homebrew e arquivos que você tenha autorização para distribuir. Não contorne DRM nem distribua jogos comerciais sem autorização.
