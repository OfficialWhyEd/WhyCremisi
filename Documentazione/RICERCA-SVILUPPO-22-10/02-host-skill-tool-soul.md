# 02 - Host per skill, tool, soul e memoria (WhyCremisi)

Data: 22/10. Ricerca web e lettura locale. Nessuna installazione.

## 1. Standard aperti per caricare skill e tool

**Agent Skills (SKILL.md)**
- Una skill e' una cartella con un file SKILL.md obbligatorio: frontmatter YAML + corpo Markdown.
- Campi: `name` (obbligatorio, max 64 car., solo a-z 0-9 e trattino, uguale al nome cartella), `description` (obbligatorio, max 1024), opzionali `license`, `compatibility`, `metadata`, `allowed-tools` (sperimentale).
- Cartelle opzionali: `scripts/`, `references/`, `assets/`.
- Caricamento a tre livelli: metadati all'avvio (circa 100 token), corpo SKILL.md quando la skill si attiva (consigliato sotto 5000 token, file sotto 500 righe), risorse solo quando servono.
- Spec: https://agentskills.io/specification - repo: https://github.com/agentskills/agentskills (validatore `skills-ref`). Licenza della spec: da verificare nel repo (non confermata in questa ricerca).
- Origine Anthropic, poi standard aperto. Adottato da piu' piattaforme (fonti secondarie dicono 20 o piu').

**MCP (Model Context Protocol)**
- Protocollo per collegare tool, risorse e prompt a un agente tramite JSON-RPC 2.0. Trasporti: stdio e HTTP/SSE (streamable HTTP nelle versioni recenti).
- Spec e repo: https://github.com/modelcontextprotocol/modelcontextprotocol - LICENZA MIT (verificato sulla pagina del repo).
- SDK ufficiali (TypeScript, Python, C#, Java, Rust, PHP, Ruby): licenze MIT nella maggior parte dei casi, da verificare per ciascuno.
- SDK C++ ufficiale: NON trovato. Esistono progetti terzi:
  - https://github.com/Neumann-Labs/mcp-cpp - C++17, early stage, client/server HTTP-SSE ancora in lavorazione. Licenza: file LICENSE, non letto.
  - https://github.com/ArcadeAI/mcpp - client C++20 con stdio e HTTP/SSE. Licenza: non verificata.
  - hkr04/cpp-mcp - segnalato come MIT (FreeBSD ports, fonte secondaria), target protocollo 2024-11-05.
- Per un host minimale in C++ basta implementare: framing JSON-RPC su stdio, handshake `initialize`, `tools/list`, `tools/call`. Il resto e' opzionale.

**AGENTS.md**
- Formato Markdown alla radice del repo con istruzioni per agenti di coding. Spec: https://github.com/openai/agents.md (licenza MIT segnalata da fonti secondarie). Ora sotto la Linux Foundation (Agentic AI Foundation), secondo fonti secondarie.
- Regola pratica: l'agente legge il file piu' vicino nell'albero delle cartelle. Il prompt utente vince sul file.

**OpenClaw e Hermes (formati dell'ecosistema)**
- OpenClaw: framework agente in TypeScript, licenza MIT (fonte secondaria, da verificare su https://github.com/openclaw). Skill su ClawHub (https://clawhub.ai), CLI MIT. Attenzione: nel registro sono stati segnalati skill malevole (incidente "ClawHavoc", 341 skill secondo fonti secondarie). Ogni skill e' codice che gira con i privilegi dell'agente.
- Hermes Agent (Nous Research): MIT segnalata da fonti secondarie. Usa SKILL.md compatibili con agentskills.io. Memoria in Markdown locale (~/.hermes/, memory.md, user.md). Verificare il repo ufficiale prima di copiare.

## 2. Repo open source per host e loader

| Progetto | Cosa fa | Licenza | Nota per WhyCremisi |
|---|---|---|---|
| agentskills/agentskills | Spec SKILL.md + validatore skills-ref | da verificare nel repo | Standard da adottare |
| modelcontextprotocol (spec) | Protocollo MCP | MIT (verificato) | Protocollo da parlare |
| Neumann-Labs/mcp-cpp | Client/server MCP in C++17 | da LICENSE | Acerbo, ma C++ puro |
| ArcadeAI/mcpp | Client MCP C++20 | da verificare | Base possibile per il client |
| openai/agents.md | Spec AGENTS.md | MIT (fonti secondarie) | Formato istruzioni progetto |
| Soundux/webviewpp | Wrapper WebView2 in CMake | da verificare | Riferimento per WebView in JUCE |
| JUCE (WebBrowserComponent) | Web view con flag JUCE_USE_WIN_WEBVIEW2 | GPLv3 o licenza commerciale JUCE | Il bridge pagina-C++ e' un messaggio JSON |

Nota: non ho trovato un progetto che unisca JUCE + WebView2 + JSON-RPC + host MCP. Va costruito, con i pezzi sopra. Il pattern di messaggi bidirezionali WebView si puo' prendere da esempi WebView2 (postMessage e handler lato host).

## 3. Percorsi reali sul PC (solo conteggi, nessuna chiave letta)

Cartelle skill trovate:
- `C:\Users\Whyed\.claude\skills` - 119 voci
- `C:\Users\Whyed\.claude\agents` - 25 voci
- `C:\Users\Whyed\.claude\plugins` - 11 voci (marketplace ufficiale con 40 plugin in `marketplaces\claude-plugins-official\plugins`)
- `C:\Users\Whyed\.codex\skills` - 103 voci; `C:\Users\Whyed\.codex\.tmp\plugins\plugins` - 120 voci
- `C:\Users\Whyed\.agents\skills` - 26 voci
- `C:\Users\Whyed\.config\opencode\skills` - 21 voci
- `C:\Users\Whyed\.opencode`, `C:\Users\Whyed\.openclaw`, `C:\Users\Whyed\.hermes`: NON esistono nel profilo utente.
- `E:\Dev\SkillMotion\sorgenti\` - 15 sorgenti di skill (calesthio_OpenMontage con `.agents\skills` da 90 voci e `.claude\skills` da 49 voci, heygen hyperframes 8 voci, elevenlabs, genart, p5, threejs, ecc.)
- `E:\Cervello\1 DAL MAC\OPENCLAW-DAL-MAC\home\` - `.openclaw` (45 voci), `.claude\skills` (68), `.codex\skills` (12), `.cursor\skills` (11), `.gemini\skills` (11), `.openclaw\agents` (4)
- `E:\Cervello\1 DAL MAC\estratto\CERVELLO-MAC\.claude\skills` - 69 voci
- `E:\.openclaw.copia` - copia di OpenClaw in radice E:\ (non esplorata nel dettaglio)

Altre cartelle utili in E:\ : Dev, Cervello, Lavori, Chiavi (NON letta, contiene segreti).

## 4. Soul, identity, memory in file Markdown (stile OpenClaw)

File per l'agente, tutti in Markdown, caricati a inizio sessione:
- **SOUL.md**: carattere, tono, valori. Regole di comportamento stabili. Esempio: "non riscrivere la memoria, solo aggiungere".
- **IDENTITY.md**: nome, ruolo, creatore, confini operativi.
- **AGENTS.md**: istruzioni per il lavoro (come usare tool e skill). Scritto dall'uomo.
- **USER.md**: profilo della persona (preferenze, stile, cose da sapere).
- **MEMORY.md**: memoria curata e breve. Scrittura solo per aggiunta o curata, mai sovrascritta a caso.
- **DREAMS.md** (o diario): log dei cicli di consolidamento.

Limiti di caricamento (fonte secondaria su OpenClaw): circa 20.000 caratteri per file e 150.000 in totale per i file di bootstrap.

**Dreaming (consolidamento memoria)**
- Passo in background, opzionale. Legge i segnali a breve termine (log giornalieri), assegna un punteggio e promuove solo gli elementi qualificati in MEMORY.md.
- Ogni ciclo scrive un riassunto leggibile in DREAMS.md, cosi' l'utente puo' rivedere.
- Pre-compaction flush: prima che il contesto si riempia, un turno silenzioso chiede al modello di salvare il contesto importante nei file di memoria.
- Fonti: https://docs.openclaw.ai/concepts/memory-architecture (ufficiale, letta solo in ricerca) e guide di terze parti.

## 5. Raccomandazione

1. Usare il formato SKILL.md (agentskills.io) come unico formato skill. Cosi' le skill di Claude Code, Codex, OpenCode, Hermes e OpenClaw sono leggibili senza conversione.
2. Parlare MCP con un client C++ minimale (stdio JSON-RPC: initialize, tools/list, tools/call). Partire da mcp-cpp o mcpp solo dopo aver letto la licenza. Se la licenza non e' permissiva, implementare il client da zero (sono poche centinaia di righe).
3. Memoria: SOUL.md, IDENTITY.md, AGENTS.md, USER.md, MEMORY.md, con dreaming a fine sessione e pre-compaction flush.
4. Sicurezza: le skill di terze parti vanno in quarantena (niente esecuzione automatica di script senza conferma), dato il caso ClawHavoc.
5. Da verificare prima di usare: licenze di agentskills, mcp-cpp, mcpp, OpenClaw, Hermes (sono state segnalate solo da fonti secondarie).

## Fonti

- https://agentskills.io/specification (letta)
- https://github.com/modelcontextprotocol/modelcontextprotocol (licenza MIT letta)
- https://github.com/Neumann-Labs/mcp-cpp
- https://github.com/ArcadeAI/mcpp
- https://docs.openclaw.ai/concepts/memory-architecture
- https://hermes-agent.nousresearch.com/docs/user-guide/skills/bundled/autonomous-ai-agents/autonomous-ai-agents-hermes-agent
- https://agentic-ai.readthedocs.io/en/latest/Standards/agents-md/
- https://forum.juce.com/t/webview2-with-juce-7-0-3-and-cmake/54015/2
