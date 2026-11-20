# NEXUS STORE - PS4

## Funcionalidades da versao 0.20
- Interface redesenhada com fonte Inter, paineis e botoes arredondados.
- Aba CONECTAR com QR dinamico por endereco IP e chave de pareamento.
- Pagina para celular com apenas um campo: link direto do PKG.
- O app encaminha o link ao Remote Package Install local do PS4 (API v2, com fallback ao endpoint legado).
- Historico local de envios agrupado por TITLE ID e reconhecimento heuristico de updates pelo nome/URL.
- O nome do pacote e inferido automaticamente a partir do nome do arquivo.

## Requisito de instalacao
O PS4 precisa estar na mesma rede Wi-Fi do celular e com o servico Remote Package Install ativo (GoldHEN Remote Package Install ou Remote Package Installer na porta 12800). O NEXUS STORE nao contorna DRM nem remove protecoes de pacotes.

A identificacao de atualizacoes usa TITLE ID quando ele aparece no link ou na resposta do instalador; quando o servidor usa URL opaca, o agrupamento pode ficar limitado ao nome do arquivo.

## Build
O workflow baixa SDL2, FreeType, a fonte Inter (SIL Open Font License) e o gerador QR Code de codigo aberto. O pacote so deve ser considerado validado apos build concluido e teste no PS4 alvo.
