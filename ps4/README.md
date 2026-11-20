# NEXUS STORE - PS4

## Funcionalidades da versao 0.20
- Interface redesenhada com fonte Inter, paineis e botoes arredondados.
- Aba CONECTAR com QR dinamico por endereco IP e chave de pareamento.
- Pagina para celular com apenas um campo: link direto do PKG.
- O app carrega os módulos internos UserService, AppInstUtil e BGFT pelos IDs nativos do sistema, registra o link diretamente no BGFT e inicia o download/instalação sem Remote Package Installer externo.
- Historico local de envios agrupado por TITLE ID e reconhecimento heuristico de updates pelo nome/URL.
- O nome do pacote e inferido automaticamente a partir do nome do arquivo.

## Requisito de instalacao
O PS4 e o celular precisam estar na mesma rede Wi-Fi. O aplicativo usa o serviço nativo BGFT do sistema para registrar e iniciar o download do PKG; não usa a porta 12800 nem depende do aplicativo Remote Package Installer. O GoldHEN precisa estar ativo e o perfil do PS4 deve estar conectado. O carregamento do BGFT usa o sysmodule interno 0x8000002A, não um caminho de arquivo SPRX. O NEXUS STORE não contorna DRM nem remove proteções de pacotes. A chamada nativa ainda precisa ser validada no console físico e pode variar conforme firmware/versão do GoldHEN.

A identificacao de atualizacoes usa TITLE ID quando ele aparece no link ou na resposta do instalador; quando o servidor usa URL opaca, o agrupamento pode ficar limitado ao nome do arquivo.

## Build
O workflow baixa SDL2, FreeType, a fonte Inter (SIL Open Font License) e o gerador QR Code de codigo aberto. O pacote so deve ser considerado validado apos build concluido e teste no PS4 alvo.
