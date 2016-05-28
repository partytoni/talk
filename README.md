Il servizio tiene traccia di un elenco di client pronti ad iniziare una
"conversazione".
I client (residenti, in generale, su macchine diverse), dopo essersi connessi
al servizio acquisiscono l'elenco e lo mostrano all'utente il quale potra'
collegarsi a un altro utente disponibile iniziando cosi' la conversazione.
Due client collegati tra loro permetteranno ai relativi utenti di scambiarsi
messaggi finche' uno dei due non interrompe la conversazione, e saranno
indisponibili ad altre conversazioni. Al termine di una conversazione i
client ritorneranno disponibili fino a che non si scollegano dal server.

Per compilare i file e usufruire del servizio talk (sia server che client) basta eseguire
lo script install.sh dalla cartella principale che genererà una cartella build/,contentente
i file necessari durante compilazione, ed una cartella run/ dove ci saranno gli
eseguibili di server e client.

riepilogo dei passi di compilazione ed esecuzione via terminale :
1 rendere eseguibile (se non lo è) install.sh:
### > chmod +x install.sh
2 eseguire lo script di installazione
### > ./install.sh
3 entrare nella cartella run/ e laniciare l'eseguibile desiderato (client o server)
### > cd run/
### > ./client --help
