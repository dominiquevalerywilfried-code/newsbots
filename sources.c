/* sources.c - Toutes tes sources configurées
 *
 * Pour ajouter une source :
 *   { URL, NOM, "", "", "", 0, FREQ, IS_RSS, 1 }
 *
 * IDs YouTube trouvés :
 *   Astronophilos       : UC0zM_qfkIBw551p9WzCJ38g
 *   SpaceX              : UCtI0Hodo5o5dUb67FeUjDeA
 *   Le Journal Starbase : UC1gz6sxUY-P5nRRhBV5o1ew
 *   Le Journal Espace   : @LeJournaldelEspace (handle)
 */

#include "newsbot.h"

/* Macro pour construire l'URL RSS YouTube */
#define YT_RSS(id) \
    "https://www.youtube.com/feeds/videos.xml?channel_id=" id

Source DEFAULT_SOURCES[] = {

    /* ══════════════ ESPACE & NEW SPACE ══════════════ */
    {
        YT_RSS("UCtI0Hodo5o5dUb67FeUjDeA"),
        "SpaceX YouTube", "", "", "", 0,
        FREQ_HIGH, 1, 1
    },
    {
        "https://www.spacex.com/updates",
        "SpaceX Updates", "", "", "", 0,
        FREQ_HIGH, 0, 1
    },
    {
        YT_RSS("UC0zM_qfkIBw551p9WzCJ38g"),
        "Astronophilos", "", "", "", 0,
        FREQ_MEDIUM, 1, 1
    },
    {
        YT_RSS("UC1gz6sxUY-P5nRRhBV5o1ew"),
        "Journal de la Starbase (JDS)", "", "", "", 0,
        FREQ_HIGH, 1, 1
    },
    {
        /* Handle YouTube → on résout l'ID au runtime si besoin */
        "https://www.youtube.com/feeds/videos.xml?channel_id=UCeA3_9WGKB9MKkHBHBmwkfg",
        "Le Journal de l'Espace (JDE)", "", "", "", 0,
        FREQ_MEDIUM, 1, 1
    },

    /* ══════════════ BLUE ORIGIN & CONCURRENTS ══════════════ */
    {
        "https://www.blueorigin.com/news",
        "Blue Origin News", "", "", "", 0,
        FREQ_MEDIUM, 0, 1
    },
    {
        "https://www.rocketlabusa.com/updates/",
        "Rocket Lab Updates", "", "", "", 0,
        FREQ_MEDIUM, 0, 1
    },
    {
        "https://www.skyroot.in/blog",
        "Skyroot Aerospace Blog", "", "", "", 0,
        FREQ_LOW, 0, 1
    },

    /* ══════════════ ASTRONOMIE & SCIENCE ══════════════ */
    {
        "https://www.nasa.gov/rss/dyn/breaking_news.rss",
        "NASA Breaking News", "", "", "", 0,
        FREQ_HIGH, 1, 1
    },
    {
        "https://www.esa.int/rssfeed/Our_Activities/Space_Science",
        "ESA Space Science", "", "", "", 0,
        FREQ_MEDIUM, 1, 1
    },
    {
        "https://spaceweather.com/",
        "Space Weather", "", "", "", 0,
        FREQ_HIGH, 0, 1
    },

    /* ══════════════ AERONAUTIQUE ══════════════ */
    {
        "https://aviationweek.com/rss.xml",
        "Aviation Week", "", "", "", 0,
        FREQ_MEDIUM, 1, 1
    },
    {
        "https://www.flightglobal.com/rss",
        "Flight Global", "", "", "", 0,
        FREQ_MEDIUM, 1, 1
    },

    /* ══════════════ TECH & IA ══════════════ */
    {
        "https://techcrunch.com/feed/",
        "TechCrunch", "", "", "", 0,
        FREQ_MEDIUM, 1, 1
    },
    {
        "https://www.theverge.com/rss/index.xml",
        "The Verge", "", "", "", 0,
        FREQ_MEDIUM, 1, 1
    },
    {
        "https://news.ycombinator.com/rss",
        "Hacker News", "", "", "", 0,
        FREQ_HIGH, 1, 1
    },
    {
        "https://openai.com/blog/rss/",
        "OpenAI Blog", "", "", "", 0,
        FREQ_MEDIUM, 1, 1
    },
    {
        "https://www.anthropic.com/rss.xml",
        "Anthropic Blog", "", "", "", 0,
        FREQ_MEDIUM, 1, 1
    },

    /* ══════════════ NVIDIA ══════════════ */
    {
        "https://blogs.nvidia.com/feed/",
        "NVIDIA Blog", "", "", "", 0,
        FREQ_MEDIUM, 1, 1
    },
    {
        "https://www.reddit.com/r/nvidia/.rss",
        "Reddit r/nvidia", "", "", "", 0,
        FREQ_HIGH, 1, 1
    },

    /* ══════════════ ÉCONOMIE & POLITIQUE ══════════════ */
    {
        "https://www.lemonde.fr/economie/rss_full.xml",
        "Le Monde Économie", "", "", "", 0,
        FREQ_MEDIUM, 1, 1
    },
    {
        "https://www.lesechos.fr/rss/rss_une.xml",
        "Les Échos", "", "", "", 0,
        FREQ_MEDIUM, 1, 1
    },
    {
        "https://feeds.bbci.co.uk/news/business/rss.xml",
        "BBC Business", "", "", "", 0,
        FREQ_MEDIUM, 1, 1
    },
    {
        "https://www.france24.com/fr/rss",
        "France 24", "", "", "", 0,
        FREQ_HIGH, 1, 1
    },

    /* ══════════════ JEUX VIDÉO ══════════════ */
    {
        "https://www.jeuxvideo.com/rss/rss.xml",
        "Jeux Vidéo.com", "", "", "", 0,
        FREQ_MEDIUM, 1, 1
    },
    {
        "https://www.ign.com/articles?startIndex=0&count=10&tags=news",
        "IGN News", "", "", "", 0,
        FREQ_MEDIUM, 0, 1
    },
    {
        "https://www.reddit.com/r/gaming/.rss",
        "Reddit r/gaming", "", "", "", 0,
        FREQ_HIGH, 1, 1
    },
    {
        "https://store.steampowered.com/feeds/news/",
        "Steam News", "", "", "", 0,
        FREQ_MEDIUM, 1, 1
    },

    /* ══════════════ INDIA SPACE ══════════════ */
    {
        "https://www.isro.gov.in/rss-feed",
        "ISRO News", "", "", "", 0,
        FREQ_MEDIUM, 1, 1
    },
    {
        "https://www.reddit.com/r/ISRO/.rss",
        "Reddit r/ISRO", "", "", "", 0,
        FREQ_MEDIUM, 1, 1
    },

    /* ══════════════ SOURCES REDDIT ESPACE ══════════════ */
    {
        "https://www.reddit.com/r/spacex/.rss",
        "Reddit r/SpaceX", "", "", "", 0,
        FREQ_HIGH, 1, 1
    },
    {
        "https://www.reddit.com/r/space/.rss",
        "Reddit r/space", "", "", "", 0,
        FREQ_HIGH, 1, 1
    },
    {
        "https://www.reddit.com/r/astronomy/.rss",
        "Reddit r/astronomy", "", "", "", 0,
        FREQ_MEDIUM, 1, 1
    },

};

int DEFAULT_SOURCES_COUNT = sizeof(DEFAULT_SOURCES) / sizeof(Source);
