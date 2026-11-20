# NEXUS STORE — PS4 Homebrew

Primeira etapa do projeto: painel web administrativo e catálogo remoto.

## Configuração
- Repositório: `caiooshorts-cell/NEXUS-STORE-PS4`
- Hospedagem dos PKGs: servidores externos com links HTTP/HTTPS diretos
- Rede-alvo do console: Wi-Fi
- Destino planejado: armazenamento interno do PS4
- GitHub Pages publica o painel e `catalog.json`; os arquivos PKG não passam pelo GitHub.

## Arquivos
- `admin.html`: painel web para cadastrar/remover itens do catálogo.
- `catalog.json`: catálogo público consumido futuramente pelo app PS4.
- `.github/workflows/pages.yml`: publicação automática no GitHub Pages.

## Como ativar
1. Envie os arquivos deste pacote para a raiz do repositório, preservando a pasta `.github`.
2. Em Settings → Pages, selecione GitHub Actions como fonte, se solicitado.
3. Aguarde a execução de `Publish NEXUS STORE panel` em Actions.
4. Acesse `https://caiooshorts-cell.github.io/NEXUS-STORE-PS4/`.

## Segurança do painel
O painel pede um fine-grained GitHub token com acesso apenas a este repositório e permissão Contents: Read and write. O token fica somente na memória da aba e não é gravado no navegador. Não compartilhe o token e evite usar o painel em computadores públicos.

## Próxima etapa: aplicativo nativo PS4
Este pacote ainda NÃO é um PKG instalável. A aplicação PS4 nativa será uma etapa separada, usando toolchain homebrew apropriada e validando a integração com GoldHEN/firmware no console. O download rápido será implementado no cliente nativo: transferência direta da URL de origem, gravação em arquivo temporário no armazenamento interno, progresso/MB/s, retomada apenas com suporte HTTP Range e verificação SHA-256 antes da instalação.

Use somente homebrew e conteúdos que você tenha autorização para distribuir. Não use a loja para contornar DRM ou distribuir conteúdo não autorizado.
