Relit le code et test la bibliothèque ThreadSafe
Cree différents scénarios et execute les.
Focus sur:
* La robustesse.
  * Test individuellement chacun des traits
    * is_synchronizable<T>
    * is_synchronizable<const T>
    * is_lifetime_aware<T>
    * is_sendable<T>
  * Test individuellement les helpers
    * copy_on_write<T>
    * synchronized_value<T>
    * asynchronous_task<T> ne doit pas accepter de type unsafe
* La simplicité du code qui a vocation d'être éducationnel
  * Le code doit être simple, facile à lire et explicite
* Thread Safety
  * Pas de data race
  * Difficile d'avoir des race conditions
* Performance à la compilation
  * Les checks étant fait à la compilation, c'est normal que ça prend du temps, mais Si possibilités d'améliorer, liste les
* Performance au run time
* API facile à utiliser
* Flexibilité

Ecrit les différents rapports dans docs/
Plusieurs fichiers peuvent être générés au besoin.
Si code problématique: L'écrire complètement dans le rapport, pas juste un snippet.
Si solution: L'écrire complètement dans le rapport également