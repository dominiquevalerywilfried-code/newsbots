/* main.c - Boucle principale du NewsBot
 *
 * Ce fichier orchestre tout :
 *   1. Charge les sources
 *   2. Crawl en boucle infinie (toutes les N secondes selon la source)
 *   3. Poll Discord toutes les 5 secondes pour détecter "!go"
 *   4. Sur "!go" : résume les articles non envoyés et poste sur Discord
 */

#include "newsbot.h"
#include <signal.h>
#include <unistd.h>
#include <curl/curl.h>

/* Sources déclarées dans sources.c */
extern Source DEFAULT_SOURCES[];
extern int    DEFAULT_SOURCES_COUNT;

/* Signal handler pour arrêt propre */
static volatile int running = 1;
static void on_signal(int sig) { (void)sig; running = 0; }

/* ─── Traite une source : fetch + parse + stocke ─── */
static void process_source(Source *src)
{
    time_t now = time(NULL);

    /* Pas encore l'heure de revérifier */
    if (src->last_checked > 0 &&
        (now - src->last_checked) < src->check_interval)
        return;

    log_info("Vérification : %s", src->name);

    HttpResponse *resp = http_fetch(src->url,
                                    src->last_etag,
                                    src->last_modified);
    if (!resp) {
        log_error("Impossible de fetcher : %s", src->url);
        return;
    }

    src->last_checked = now;

    /* 304 = rien de nouveau */
    if (resp->http_code == 304) {
        log_info("  → Pas de changement (304)");
        http_response_free(resp);
        storage_save_source(src);
        return;
    }

    if (!resp->data || resp->size == 0) {
        http_response_free(resp);
        storage_save_source(src);
        return;
    }

    /* Vérifie le hash pour les pages HTML */
    char new_hash[MAX_HASH_LEN];
    sha256_string(resp->data, resp->size, new_hash);

    if (src->content_hash[0] &&
        strcmp(src->content_hash, new_hash) == 0) {
        log_info("  → Contenu identique (hash)");
        http_response_free(resp);
        storage_save_source(src);
        return;
    }

    /* Met à jour les métadonnées */
    strncpy(src->content_hash,  new_hash,         sizeof(src->content_hash) - 1);
    strncpy(src->last_etag,     resp->etag,        sizeof(src->last_etag) - 1);
    strncpy(src->last_modified, resp->last_modified,sizeof(src->last_modified) - 1);

    /* ── Parse selon le type ── */
    Article articles[64];
    int count = 0;

    if (src->is_rss) {
        count = parse_rss(resp->data, src->name, articles, 64);
        log_info("  → %d articles RSS trouvés", count);
    } else {
        /* Page HTML : extrait le texte et crée un article "résumé de page" */
        char text[MAX_DESC_LEN];
        extract_text_from_html(resp->data, text, sizeof(text));

        if (strlen(text) > 50) {
            Article *a = &articles[0];
            memset(a, 0, sizeof(Article));
            snprintf(a->title, sizeof(a->title), "Mise à jour : %s", src->name);
            strncpy(a->url,         src->url,   sizeof(a->url) - 1);
            strncpy(a->source_name, src->name,  sizeof(a->source_name) - 1);
            strncpy(a->description, text,       sizeof(a->description) - 1);
            a->published = now;
            count = 1;
            log_info("  → Page HTML mise à jour détectée");
        }
    }

    /* Stocke les nouveaux articles */
    int saved = 0;
    for (int i = 0; i < count; i++) {
        if (!storage_url_seen(articles[i].url)) {
            storage_save_article(&articles[i]);
            saved++;
        }
    }
    if (saved > 0)
        log_info("  → %d nouveaux articles sauvegardés", saved);

    http_response_free(resp);
    storage_save_source(src);
}

/* ─── Génère et envoie le résumé sur Discord ─── */
static void send_summary(void)
{
    log_info("=== Génération du résumé demandée ===");

    discord_send("⏳ Je prépare ton résumé, un instant...");

    Article unsent[MAX_ARTICLES];
    int count = storage_load_unsent(unsent, MAX_ARTICLES);

    log_info("%d articles non envoyés trouvés", count);

    if (count == 0) {
        discord_send("📭 Aucune nouvelle depuis la dernière fois. Reviens plus tard !");
        return;
    }

    /* Limite à 30 articles pour ne pas exploser le prompt */
    int to_send = count > 30 ? 30 : count;

    /* Trie par date (les plus récents en premier) — tri à bulles simple */
    for (int i = 0; i < to_send - 1; i++) {
        for (int j = 0; j < to_send - i - 1; j++) {
            if (unsent[j].published < unsent[j+1].published) {
                Article tmp  = unsent[j];
                unsent[j]    = unsent[j+1];
                unsent[j+1]  = tmp;
            }
        }
    }

    /* Résumé via Groq */
    char summary[8192];
    int ret = summarize_articles(unsent, to_send, summary, sizeof(summary));

    if (ret < 0) {
        discord_send("❌ Erreur lors de la génération du résumé. Vérifie les logs.");
        return;
    }

    /* Ajoute un header et les liens */
    char full_message[10000];
    int offset = 0;

    offset += snprintf(full_message + offset, sizeof(full_message) - offset,
        "📰 **RÉSUMÉ DE L'ACTU** (%d articles)\n"
        "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n"
        "%s\n\n"
        "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"
        "🔗 **LIENS DIRECTS** :\n",
        to_send, summary);

    for (int i = 0; i < to_send && offset < (int)sizeof(full_message) - 200; i++) {
        offset += snprintf(full_message + offset, sizeof(full_message) - offset,
            "• %s → <%s>\n",
            unsent[i].title, unsent[i].url);
    }

    if (count > 30) {
        offset += snprintf(full_message + offset, sizeof(full_message) - offset,
            "\n_(+ %d autres articles disponibles, retape !go pour la suite)_\n",
            count - 30);
    }

    discord_send(full_message);

    /* Marque comme envoyés */
    for (int i = 0; i < to_send; i++)
        storage_mark_sent(unsent[i].url);

    log_info("Résumé envoyé (%d articles)", to_send);
}

/* ─── Point d'entrée ─────────────────────────────────────── */
int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    log_info("╔══════════════════════════════════════╗");
    log_info("║         NEWSBOT v1.0 démarré         ║");
    log_info("╚══════════════════════════════════════╝");

    /* Init libcurl */
    curl_global_init(CURL_GLOBAL_ALL);

    /* Init stockage */
    if (storage_init() < 0) {
        log_error("Échec initialisation stockage");
        return 1;
    }

    /* Charge les sources sauvegardées (ETag, dates...) */
    Source saved_state[MAX_SOURCES];
    int    saved_count = storage_load_sources(saved_state, MAX_SOURCES);

    /* Fusionne avec les sources par défaut */
    Source sources[MAX_SOURCES];
    int    source_count = 0;

    for (int i = 0; i < DEFAULT_SOURCES_COUNT && source_count < MAX_SOURCES; i++) {
        sources[source_count] = DEFAULT_SOURCES[i];

        /* Récupère l'état sauvegardé si disponible */
        for (int j = 0; j < saved_count; j++) {
            if (strcmp(saved_state[j].url, DEFAULT_SOURCES[i].url) == 0) {
                strncpy(sources[source_count].last_etag,
                        saved_state[j].last_etag,
                        sizeof(sources[0].last_etag) - 1);
                strncpy(sources[source_count].last_modified,
                        saved_state[j].last_modified,
                        sizeof(sources[0].last_modified) - 1);
                strncpy(sources[source_count].content_hash,
                        saved_state[j].content_hash,
                        sizeof(sources[0].content_hash) - 1);
                sources[source_count].last_checked = saved_state[j].last_checked;
                break;
            }
        }

        source_count++;
    }

    log_info("%d sources chargées", source_count);
    log_info("En attente de '!go' sur Discord...");

    discord_send(
        "🤖 **NewsBot démarré !**\n"
        "Je surveille l'actu en continu.\n"
        "Tape `!go` quand tu veux ton résumé !"
    );

    /* ── Boucle principale ── */
    int source_index  = 0;
    int discord_ticks = 0;

    while (running) {
        /* Traite une source par itération (round-robin) */
        if (source_count > 0) {
            process_source(&sources[source_index]);
            source_index = (source_index + 1) % source_count;
        }

        /* Vérifie Discord toutes les 5 secondes */
        discord_ticks++;
        if (discord_ticks >= 5) {
            discord_ticks = 0;
            if (discord_listen_for_go())
                send_summary();
        }

        sleep(1);
    }

    log_info("Arrêt propre du NewsBot.");
    discord_send("🔴 NewsBot arrêté.");
    curl_global_cleanup();
    return 0;
}
