// Crea la coppia di chiavi che firma gli aggiornamenti di WhyCremisi (Ed25519). Si fa UNA volta.
// La privata va in E:\Chiavi (mai su GitHub), la pubblica nel repo (e finisce dentro il plugin).
// Uso:  node Tools/firma/genera-chiave.mjs
import { generateKeyPairSync } from "node:crypto";
import { existsSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const privata = process.env.WHYCREMISI_CHIAVE_FIRMA || "E:\Chiavi\whycremisi-update-ed25519.pem";
const pubblica = join(dirname(fileURLToPath(import.meta.url)), "..", "installer", "chiave-pubblica-aggiornamenti.hex");
if (existsSync(privata)) { console.log("La chiave privata esiste già: non la tocco."); process.exit(0); }
const { publicKey, privateKey } = generateKeyPairSync("ed25519");
mkdirSync(dirname(privata), { recursive: true });
writeFileSync(privata, privateKey.export({ type: "pkcs8", format: "pem" }), { mode: 0o600 });
writeFileSync(pubblica, publicKey.export({ type: "spki", format: "der" }).subarray(-32).toString("hex"));
console.log("Fatto. Privata in " + privata + " (copiala anche in un posto sicuro). Pubblica scritta in " + pubblica);
