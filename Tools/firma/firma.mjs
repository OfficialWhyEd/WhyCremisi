// Firma un file con la chiave privata degli aggiornamenti e scrive <file>.sig (firma Ed25519 in base64).
// Uso:  node Tools/firma/firma.mjs <file>
import { createPrivateKey, sign } from "node:crypto";
import { readFileSync, writeFileSync } from "node:fs";

const file = process.argv[2];
if (!file) { console.error("Uso: node firma.mjs <file>"); process.exit(2); }
const percorso = process.env.WHYCREMISI_CHIAVE_FIRMA || "E:\Chiavi\whycremisi-update-ed25519.pem";
let chiave;
try { chiave = createPrivateKey(readFileSync(percorso)); }
catch { console.error("Chiave di firma non trovata in " + percorso + ". Si crea una volta con: node Tools/firma/genera-chiave.mjs"); process.exit(3); }
writeFileSync(file + ".sig", sign(null, readFileSync(file), chiave).toString("base64"));
console.log("Firmato: " + file + ".sig");
