## Rendu
Nous avons utilisé le code du Client/Serveur TCP que nous avons fait en TP pour ce projet
Les deux parties (scan horizontal et vertical) sont dans des fichiers à part dans ip_scanner.c et port_scanner.c
Les deux parties sont regroupées dans serveurTCP.c qui appelle les différentes parties en fonctions de ce que demande le client.


## Compilation
Si il est nécessaire de recompiler, il faut prendre en compte que les algorithmes sont parallelisés avec OpenMP, il faut donc compiler tous les fichiers en suivant ce modèle : gcc cible.c -o cible -O3 -fopenmp


## Comment executer
ip_scanner peut être executé avec sudo ./ip_scanner <AdresseIP> <Masque_de_sous_réseau> (ex : sudo ./ip_scanner 192.111.111.111 255.255.255.0). On obtient alors la liste de toutes les adresses IP connectées au sous-réseau

port_scanner peut être executé avec ./port_scanner <AdresseIP> 1 70000 (1 et 70000 sont des valeurs arbitraires et correspondent au range de ports à scanner, si la valeur maximal dépasse 65535 alors le port maximal scanné sera le 65535). On obtient la liste des ports ouverts de l'adresse IP ciblée.

serveurTCP s'execute avec sudo ./serveurTCP, il faut ensuite dans une autre fenêtre executer ./clientTCP <AdresseIP>
Le client peut ainsi discuter avec le serveur.
Pour fermer la discussion, le client doit envoyer STOP
Pour lancer un scan IP, le client doit envoyer IP <AdresseIP> <Masque_de_sous_réseau> dans le même format que pour ip_scanner
Pour lancer un scan des ports, le client doit envoyer PORTS <AdresseIP> 1 70000 de la même manière que pour port_scanner
Une fois un scan terminé, le client recevra un message du serveur pour lui dire qu'il à fini sa tâche et qu'il est prêt pour une nouvelle commande.


## Remarques
Le scan des AdressesIP indique si les adresses IP répondent à un ping, donc si on execute ip_scanner pendant que la commande IP du client vers le serveur est en cours, on recevra des résultats en double.
Ce cas est également possible lors de l'execution de la commande ping pendant la commande IP, il est alors possible de différencier les paquets car les numéros d'acquittement de notre fonction sont définis à 256.

Pour le scan des ports, lors d'un scan de la machine sur laquel le serveur s'exécute il est possible de voir des ports ouvert temporairement (quelques dixièmes de secondes) il serait possible de modifier cela si c'était génant en effectuant deux scans successifs avec un temps d'attente supérieur à 0.5 secondes entre les deux et de comparer les résultats pour ne renvoyer que les ports présents lors des scans 1 et 2.