// (03/11, decisão dele: "dá pra tirar essa distinção das linhas, C&C não tem isso") A árvore da USA SEM o rótulo de tipo nas linhas:
// o tipo é lido no ícone (docs/poderes/ICONES.md). As linhas são livres, na ordem dos tipos (olhos, golpe, golpe, tropas, apoio), e
// uma patente pode ter quantos poderes precisar. Os níveis de um poder ficam na mesma linha, ligados. Ver docs/poderes/USA.md §17.
// A árvore de poderes da USA (docs/poderes/USA.md), no formato da tela de poderes do protótipo do HUD.
// ic: o ícone em a/<ic>.png do protótipo; os novos estão em docs/poderes/tela/pw/ (provisórios). Os níveis de um poder ficam em
// patentes seguidas (a linha que os liga não pode passar por trás de outro poder). Uma casa (linha × patente) = um poder.
// t: o tipo (docs/poderes/ICONES.md): atk = golpe, intel = olhos, troop = tropas, sup = apoio (a família comando); pas = passivo; unl = desbloqueio.
const ROWS = [['', ''], ['', ''], ['', ''], ['', ''], ['', '']];
// (03/11, a árvore da USA em debate: docs/poderes/USA.md; os ícones em pw/ são provisórios)
const NODES = [
  { id: 'drone', t: 'intel', row: 0, rank: 1, n: 'Spy Drone', ic: 'spy_drone', cost: 1, cd: '1:30', d: 'A stealth drone circles the point for 40 s, sees 66 m and reveals hidden units. It can be shot down.' },
  { id: 'desig', t: 'intel', row: 0, rank: 2, n: 'Target Designation', ic: 'orbital_lance', cost: 1, cd: '1:30', d: 'A satellite paints a 30 m circle for 20 s. Hidden units are revealed and enemies inside take 25% more damage. The enemy sees the laser dots.' },
  { id: 'a10_1', t: 'atk', row: 1, rank: 3, n: 'A-10 Strike', lv: 'I', ic: '../../../../poderes/tela/pw/usa_a10', cost: 2, cd: '3:00', d: 'One A-10 makes a pass: six Mavericks and the 30 mm cannon along an 80 m strip. It can be shot down.' },
  { id: 'a10_2', t: 'atk', row: 1, rank: 4, n: 'A-10 Strike', lv: 'II', req: 'a10_1', short: 'Two A-10s', ic: '../../../../poderes/tela/pw/usa_a10', cost: 1, cd: '3:00', d: 'Two A-10s side by side.' },
  { id: 'ac130', t: 'atk', row: 1, rank: 5, n: 'AC-130 Gunship', ic: '../../../../poderes/tela/pw/usa_ac130', cost: 3, cd: '6:00', d: 'Orbits the area for 12 s and fires where you click: the 105 mm howitzer and the 25 mm cannon.' },
  { id: 'para1', t: 'troop', row: 3, rank: 1, n: 'Paradrop', lv: 'I', ic: 'paradrop', cost: 1, cd: '3:00', d: 'Three Rangers and two Javelins drop at the target. $650 per drop.' },
  { id: 'para2', t: 'troop', row: 3, rank: 2, n: 'Paradrop', lv: 'II', req: 'para1', short: '6 + 4', ic: 'paradrop', cost: 1, cd: '3:00', d: 'Six Rangers and four Javelins. $1,300 per drop.' },
  { id: 'seal', t: 'troop', row: 3, rank: 4, n: 'SEAL Insertion', ic: 'p_usa_ranger', cost: 2, cd: '4:00', d: 'An Osprey lands four hidden SEALs. Their charges bring a building down from where they were planted. $600 per insertion.' },
  { id: 'para3', t: 'troop', row: 3, rank: 3, n: 'Paradrop', lv: 'III', req: 'para2', short: 'HALO, 10 + 6', ic: 'paradrop', cost: 1, cd: '3:00', d: 'HALO jump: ten Rangers and six Javelins from high altitude, where only long-range air defense reaches the plane. $2,000 per drop.' },
  { id: 'naval1', t: 'atk', row: 2, rank: 2, n: 'Naval Gunfire', lv: 'I', ic: '../../../../poderes/tela/pw/usa_naval', cost: 1, cd: '3:00', d: 'A destroyer offshore walks a line of twelve 127 mm shells across the target. Everyone hears the guns first.' },
  { id: 'naval2', t: 'atk', row: 2, rank: 3, n: 'Naval Gunfire', lv: 'II', req: 'naval1', short: '3 destroyers', ic: '../../../../poderes/tela/pw/usa_naval', cost: 1, cd: '3:00', d: 'Three destroyers, three lines of shells side by side.' },
  { id: 'naval3', t: 'atk', row: 2, rank: 4, n: 'Naval Gunfire', lv: 'III', req: 'naval2', short: 'Battleship', ic: '../../../../poderes/tela/pw/usa_naval', cost: 1, cd: '5:00', d: 'The battleship: one broadside of nine 406 mm shells. Every player hears it, then the shells come in like a freight train.' },
  { id: 'moab', t: 'atk', row: 2, rank: 5, n: 'MOAB', ic: '../../../../poderes/tela/pw/usa_moab', cost: 3, cd: '7:00', d: 'The biggest bomb, dropped from an MC-130 by parachute. The mark stays on the ground for 12 s.' },
  { id: 'repair1', t: 'sup', row: 4, rank: 1, n: 'Emergency Repair', lv: 'I', ic: 'repair', cost: 1, cd: '2:00', d: 'Vehicles in the area get back 30% of their health and take 25% less damage for 15 s.' },
  { id: 'awacs', t: 'pas', row: 0, rank: 3, n: 'AWACS Early Warning', ic: '../../../../poderes/tela/pw/usa_awacs', cost: 1, cd: 'Passive', d: 'Every enemy power shows its type, route and target the moment it is called, for you and your allies.' },
  { id: 'repair2', t: 'sup', row: 4, rank: 2, n: 'Emergency Repair', lv: 'II', req: 'repair1', short: '50%, buildings too', ic: 'repair', cost: 1, cd: '2:00', d: '50% back, and buildings too.' },
  { id: 'angel', t: 'sup', row: 4, rank: 4, n: 'Guardian Angel', ic: 'armor_upgrade', cost: 2, cd: '3:00', d: 'For 20 s, lasers and interceptors shoot down missiles, rockets, shells and bombs over a 40 m circle. Not superweapons.' },
  { id: 'fob', t: 'troop', row: 3, rank: 5, n: 'Forward Base', ic: 'p_usa_war_factory', cost: 3, cd: '6:00', d: 'A C-17 lands and sets up a forward base in 20 s. Reinforcements can leave from it, and it repairs vehicles around it. One at a time.' }];
