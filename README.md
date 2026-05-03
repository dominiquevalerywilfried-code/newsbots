# 📰 NewsBot - Guide d'installation complet

Bot d'actualités personnalisé en C.
Surveille tes sources préférées et te résume l'actu sur Discord quand tu tapes `!go`.

---

## 🗂️ Structure du projet

```
newsbot/
├── main.c          → Boucle principale
├── fetcher.c       → Téléchargement HTTP (libcurl)
├── parser.c        → Parsing RSS/Atom/HTML
├── storage.c       → Sauvegarde articles et sources
├── summarizer.c    → Résumé via API Groq
├── discord.c       → Bot Discord (envoi + écoute)
├── hash.c          → SHA-256 (détection de changements)
├── log.c           → Logs horodatés
├── sources.c       → Toutes tes sources configurées
├── newsbot.h       → Structs et prototypes
└── Makefile        → Compilation
```

---

## 🖥️ Hébergement : Oracle Cloud Free Tier (recommandé)

Oracle offre un serveur Linux **gratuit à vie** :
- 1 vCPU AMD, 1 Go RAM (suffisant pour ce bot)
- Pas de carte bleue obligatoire pour le free tier

1. Va sur https://www.oracle.com/cloud/free/
2. Crée un compte
3. Crée une instance "Always Free" (Ubuntu 22.04)
4. Connecte-toi en SSH : `ssh ubuntu@ton-ip`

---

## 🔧 Installation sur le serveur

```bash
# 1. Installe les dépendances
sudo apt update
sudo apt install -y gcc make libcurl4-openssl-dev git

# 2. Clone / copie ton projet
mkdir ~/newsbot && cd ~/newsbot
# (copie tes fichiers .c et .h ici)

# 3. Compile
make

# Tu dois voir : ✅ Compilation réussie !
```

---

## 🤖 Créer ton bot Discord

1. Va sur https://discord.com/developers/applications
2. Clique **New Application** → donne un nom
3. Dans **Bot** → clique **Add Bot**
4. Copie le **Token** (garde-le secret !)
5. Dans **OAuth2 > URL Generator** :
   - Coche `bot`
   - Coche les permissions : `Send Messages`, `Read Message History`
6. Copie le lien généré, ouvre-le dans ton navigateur
7. Ajoute le bot à ton serveur Discord

**Trouver l'ID du channel :**
- Dans Discord : Paramètres → Avancé → Mode développeur ON
- Clic droit sur le channel → **Copier l'identifiant**

---

## 🔑 Obtenir la clé Groq (gratuit)

1. Va sur https://console.groq.com/
2. Crée un compte (gratuit)
3. Dans **API Keys** → **Create API Key**
4. Copie la clé

---

## 🚀 Lancer le bot

```bash
# Configure les variables d'environnement
export GROQ_API_KEY="gsk_xxxxxxxxxxxx"
export DISCORD_TOKEN="ton_token_discord"
export DISCORD_CHANNEL_ID="123456789012345678"

# Lance le bot
./newsbot
```

---

## 🔄 Lancer en permanence (même si tu fermes le terminal)

```bash
# Crée un fichier de service systemd
sudo nano /etc/systemd/system/newsbot.service
```

Contenu du fichier :
```ini
[Unit]
Description=NewsBot - Actu personnalisée
After=network.target

[Service]
Type=simple
User=ubuntu
WorkingDirectory=/home/ubuntu/newsbot
ExecStart=/home/ubuntu/newsbot/newsbot
Environment="GROQ_API_KEY=ta_clé_groq"
Environment="DISCORD_TOKEN=ton_token"
Environment="DISCORD_CHANNEL_ID=ton_channel_id"
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

```bash
# Active et démarre
sudo systemctl enable newsbot
sudo systemctl start newsbot

# Vérifie que ça tourne
sudo systemctl status newsbot

# Voir les logs en direct
journalctl -u newsbot -f
```

---

## 💬 Utilisation

Dans ton channel Discord :
- `!go` ou `!resume` → reçois le résumé des dernières actus
- Le bot poste aussi un message de bienvenue au démarrage

---

## ➕ Ajouter une source

Dans `sources.c`, ajoute une entrée :

```c
// Flux RSS
{
    "https://exemple.com/rss.xml",
    "Nom de la source", "", "", "", 0,
    FREQ_MEDIUM, 1, 1   // 1 = RSS
},

// Chaîne YouTube (remplace CHANNEL_ID)
{
    YT_RSS("UCxxxxxxxxxxxxxxxxxxxxxxxxx"),
    "Nom chaîne YouTube", "", "", "", 0,
    FREQ_MEDIUM, 1, 1
},

// Page web normale
{
    "https://exemple.com/news",
    "Site web", "", "", "", 0,
    FREQ_LOW, 0, 1      // 0 = HTML
},
```

Puis recompile : `make`

---

## ❓ FAQ

**Q : Comment trouver l'ID d'une chaîne YouTube ?**
Va sur la chaîne, clic droit → Afficher le source de la page, cherche `channel_id=`.

**Q : Le bot peut-il tourner sur mon PC ?**
Oui, mais il faut que le PC soit allumé en permanence. Oracle Cloud est mieux.

**Q : Combien ça consomme ?**
Très peu. Le bot dort 1 seconde entre chaque check. CPU < 1%, RAM < 5 Mo.

**Q : Groq peut-il tomber en panne ?**
Rarement. Si l'API est indisponible, le bot envoie un message d'erreur sur Discord.

---

## 📊 Sources configurées

| Source | Type | Fréquence |
|--------|------|-----------|
| SpaceX YouTube | RSS YouTube | 30 min |
| SpaceX.com | HTML | 30 min |
| Astronophilos | RSS YouTube | 2h |
| Journal de la Starbase | RSS YouTube | 30 min |
| Journal de l'Espace | RSS YouTube | 2h |
| Blue Origin | HTML | 2h |
| Rocket Lab | HTML | 2h |
| Skyroot Aerospace | HTML | 24h |
| NASA Breaking News | RSS | 30 min |
| ESA Space Science | RSS | 2h |
| NVIDIA Blog | RSS | 2h |
| Reddit r/SpaceX | RSS | 30 min |
| TechCrunch | RSS | 2h |
| Hacker News | RSS | 30 min |
| Le Monde Économie | RSS | 2h |
| Jeux Vidéo.com | RSS | 2h |
| Reddit r/gaming | RSS | 30 min |
| + 15 autres... | | |
