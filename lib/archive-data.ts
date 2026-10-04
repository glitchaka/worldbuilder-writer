export type ChapterEvidence = {
  chapter: string;
  text: string;
};

export type CharacterRecord = {
  id: string;
  name: string;
  imageUrl?: string;
  aliases: string[];
  category: "Principal" | "Secundario" | "Incidental" | "Histórico" | "Entidad";
  status: "Activo" | "Muerto" | "Desconocido" | "Histórico" | "Entidad";
  role: string;
  occupation: string;
  origin: string;
  affiliation: string;
  summary: string;
  background: string;
  physical: string;
  traits: string[];
  evidence: ChapterEvidence[];
  presence: number[];
  color: string;
  board: { x: number; y: number; visible: boolean };
};

export type RelationshipRecord = {
  id: string;
  source: string;
  target: string;
  type: "Familia" | "Enemigo" | "Conocido" | "Solo interactuaron" | "Amistad" | "Alianza" | "Conflicto" | "Investigación" | "Tutela" | "Romance" | "Sospecha" | "Trabajo" | "Culto";
  label: string;
  certainty: "Hecho" | "Hipótesis";
  strength: 1 | 2 | 3;
  details: string;
};

export type TheoryRecord = {
  id: string;
  title: string;
  status: "Confirmada" | "Muy probable" | "Abierta" | "Descartada";
  confidence: number;
  thesis: string;
  evidence: ChapterEvidence[];
  counterpoint: string;
  characterIds: string[];
  tags: string[];
};

export type TimelineEvent = {
  id: string;
  chapter: string;
  when: string;
  title: string;
  summary: string;
  characterIds: string[];
  intensity: number;
};

export type ArchiveTheme = "grim" | "chronicle" | "desk" | "classic" | "kawaii";

export type ThemeFont = "Antigua" | "Clásica" | "Épica" | "Moderna" | "Mecánica";

export type CustomThemeDefinition = {
  formatVersion: 1;
  id: string;
  name: string;
  description: string;
  base: ArchiveTheme;
  colors: {
    background: string;
    sidebar: string;
    surface: string;
    panel: string;
    card: string;
    ink: string;
    muted: string;
    accent: string;
    secondary: string;
    border: string;
    board: string;
  };
  typography: {
    display: ThemeFont;
    body: ThemeFont;
  };
  radius: number;
};

export type ArchiveAttachment = {
  id: string;
  name: string;
  mimeType: string;
  size: number;
  kind: "image" | "text" | "document";
  /** Embedded only by the portable desktop edition so its project folder is self-contained. */
  dataUrl?: string;
};

export type ImportedTextRecord = {
  id: string;
  title: string;
  content: string;
  sourceName: string;
  importedAt: string;
};

export type HeatmapConfig = {
  title: string;
  description: string;
  columnLabels: string[];
};

export type BoardViewport = {
  scale: number;
  x: number;
  y: number;
  /** Distinguishes pixel-based world coordinates from the legacy percentage board. */
  worldSpace?: true;
};

export const BOARD_WORLD_WIDTH = 5200;
export const BOARD_WORLD_HEIGHT = 3400;

export type ManuscriptLayout = {
  preset: "editorial" | "novela" | "bolsillo" | "fantasia" | "cronica" | "personalizada";
  pageWidthMm: number;
  pageHeightMm: number;
  marginTopMm: number;
  marginRightMm: number;
  marginBottomMm: number;
  marginLeftMm: number;
  fontFamily: "Garamond" | "Georgia" | "Literata" | "Bookerly" | "Atkinson";
  fontSizePt: number;
  lineHeight: number;
  paragraphIndentMm: number;
  chapterOpening: "Página nueva" | "Página impar" | "Continuo";
  sceneSeparator: string;
  headerText: string;
  footerText: string;
};

export type WorldMapMarker = {
  id: string;
  label: string;
  x: number;
  y: number;
  kind: "Capital" | "Ciudad" | "Ruina" | "Puerto" | "Fortaleza" | "Lugar";
};

export type WorldMapRecord = {
  id: string;
  name: string;
  description: string;
  seed: number;
  style: "Pergamino" | "Atlas" | "Nocturno";
  continents: number;
  islands: number;
  roughness: number;
  backgroundImageDataUrl?: string;
  markers: WorldMapMarker[];
  updatedAt: string;
};

export type ProjectProfile = {
  archiveTitle: string;
  storyTitle: string;
  subtitle: string;
  projectLabel: string;
  homeHeading: string;
  location: string;
  author: string;
  genre: string;
  status: string;
  synopsis: string;
  chapterLabels: string[];
  theme: ArchiveTheme;
  activeThemeId?: string;
  customThemes?: CustomThemeDefinition[];
  spotifyPlaylistUrl?: string;
  youtubeAmbientUrl?: string;
  coverImageDataUrl?: string;
  bannerImageDataUrl?: string;
  boardIconDataUrl?: string;
  boardViewport?: BoardViewport;
  sceneBoardViewport?: BoardViewport;
  sceneBoardCompact?: boolean;
  manuscriptLayout?: ManuscriptLayout;
  writingAnalysis?: WritingAnalysisSettings;
};

export type WritingAnalysisSettings = {
  repetitionWindow: number;
  fillerPhrases: string[];
};

export type WorldRecord = {
  id: string;
  kind: "País/Reino" | "Región" | "Ciudad/Lugar" | "Pueblo/Cultura" | "Moneda" | "Idioma" | "Religión" | "Organización" | "Objeto/Artefacto" | "Cosmología" | "Concepto" | "Otro";
  name: string;
  aliases: string[];
  summary: string;
  geography: string;
  government: string;
  peoples: string;
  culture: string;
  economy: string;
  currency: string;
  languages: string;
  religions: string;
  military: string;
  history: string;
  relations: string;
  locations: string;
  conflicts: string;
  notes: string;
  tags: string[];
  attachments?: ArchiveAttachment[];
};

export type MagicSystemRecord = {
  id: string;
  name: string;
  category: string;
  status: "Canónico" | "En desarrollo" | "Secreto" | "Descartado";
  source: string;
  principle: string;
  access: string;
  cost: string;
  limits: string;
  manifestations: string;
  materials: string;
  institutions: string;
  users: string;
  risks: string;
  history: string;
  notes: string;
  evidence: ChapterEvidence[];
  tags: string[];
  attachments?: ArchiveAttachment[];
};

export type WritingScene = {
  id: string;
  chapterId: string;
  order: number;
  title: string;
  content: string;
  pov: string;
  location: string;
  narrativeLayer: string;
  status: "Borrador" | "Revisión" | "Final";
  updatedAt: string;
};

export type WritingChapter = {
  id: string;
  label: string;
  title: string;
  order: number;
  scenes: WritingScene[];
};

export type ArchiveState = {
  dataVersion?: number;
  title: string;
  profile: ProjectProfile;
  manuscript: {
    fileName: string;
    words: number;
    chapters: number;
    updatedLabel: string;
  };
  characters: CharacterRecord[];
  relationships: RelationshipRecord[];
  theories: TheoryRecord[];
  timeline: TimelineEvent[];
  world: WorldRecord[];
  magicSystems: MagicSystemRecord[];
  worldTexts: ImportedTextRecord[];
  magicTexts: ImportedTextRecord[];
  writingChapters: WritingChapter[];
  maps: WorldMapRecord[];
  /** Conservado únicamente para no destruir diagnósticos de versiones anteriores. Ya no se muestra ni se exporta. */
  heatmap?: HeatmapConfig;
  /** Conservado únicamente para migración de datos anteriores. */
  narrativeHeat: Array<{ label: string; values: number[]; note: string }>;
};

export const chapterLabels = ["P", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"];

export const defaultWritingAnalysis: WritingAnalysisSettings = {
  repetitionWindow: 40,
  fillerPhrases: [
    "de repente",
    "entonces",
    "bueno",
    "en realidad",
    "de alguna manera",
    "era",
    "estaba",
    "había",
  ],
};

export const defaultManuscriptLayout: ManuscriptLayout = {
  preset: "editorial",
  pageWidthMm: 152.4,
  pageHeightMm: 228.6,
  marginTopMm: 20,
  marginRightMm: 19,
  marginBottomMm: 22,
  marginLeftMm: 19,
  fontFamily: "Garamond",
  fontSizePt: 11,
  lineHeight: 1.35,
  paragraphIndentMm: 5,
  chapterOpening: "Página nueva",
  sceneSeparator: "⁂",
  headerText: "{título}",
  footerText: "{página}",
};

const characters: CharacterRecord[] = [
  {
    id: "diego",
    name: "Diego de Montemar",
    aliases: ["Don Diego", "Hidalgo", "Tontidiego"],
    category: "Principal",
    status: "Activo",
    role: "Investigador central del caso",
    occupation: "Investigador de la Cofradía del Sacro Hierro; veterano y antiguo marinero",
    origin: "Puerto Ámbar, Thet",
    affiliation: "Casa Montemar · Sacro Hierro",
    summary: "Veterano de la Guerra de los Reinos Intermedios que persigue el patrón común entre el buwano, Felisa, los artilleros y los gemelos.",
    background: "Nació en una casa noble venida a menos. Se hizo a la mar de joven, combatió en la guerra y regresó marcado por ella. La Orden lo incorporó como investigador y lo entrenó en Hakvar. Anselmo lo formó durante su adolescencia; Clota fue su aya. Sus métodos combinan observación forense, disciplina de combate y una percepción que él atribuye al entrenamiento.",
    physical: "Alto, fuerte y bronceado; cabello castaño revuelto, canas en las sienes y ojos gris acero. Viste cuero negro, lino blanco, botas curtidas y, con frío, capa granate.",
    traits: ["Paciente", "Analítico", "Leal", "Temerario", "Orgulloso", "Hipervigilante"],
    evidence: [
      { chapter: "Prólogo", text: "Conoce a Alvargio entre los cañones malterios y los artilleros." },
      { chapter: "1", text: "El charlatán resume su pasado naval, militar y su cargo en la Orden." },
      { chapter: "10", text: "Los gemelos prueban su capacidad y concluyen que «no despertó»." },
    ],
    presence: [6, 36, 18, 2, 15, 13, 15, 24, 46, 29, 28, 20],
    color: "amber",
    board: { x: 48, y: 46, visible: true },
  },
  {
    id: "alessandra",
    name: "Alessandra",
    aliases: ["La Signora", "Alessa", "Señora De Corvo (error de Makira)"],
    category: "Principal",
    status: "Activo",
    role: "Dueña de Casa Bajamar; hermana de Felisa e investigadora por cuenta propia",
    occupation: "Regenta Casa Bajamar",
    origin: "Pasado desconocido; adoptada en Puerto Ámbar",
    affiliation: "Casa Bajamar · familia Serdán",
    summary: "Mujer de autoridad magnética que entra al caso tras la muerte de Felisa y descubre que los gemelos siguieron todo el itinerario de Diego.",
    background: "Su memoria comienza cuando le retiraron unas vendas durante su infancia. Anselmo y Yolanda la acogieron después del asalto a la casona malteria, y creció como hermana de Felisa. Construyó una posición propia como Signora de Casa Bajamar. Su vínculo con Diego mezcla intimidad, desafío y una cooperación que ninguno quiere nombrar con facilidad.",
    physical: "Porte imponente, piel apenas canela, cabello largo azabache y labios pintados de rojo muy oscuro. Usa vestidos negros, chal de encajes y perfume de jazmín, orquídea y azahar.",
    traits: ["Autoritaria", "Protectora", "Perspicaz", "Orgullosa", "Seductora", "Implacable"],
    evidence: [
      { chapter: "8", text: "Anselmo rescata de la casona a una niña sin pasado y la entrega a su esposa." },
      { chapter: "9", text: "Declara que Anselmo y su esposa la recibieron como hija." },
      { chapter: "11", text: "Confirma que su historia comienza cuando le quitaron las vendas." },
    ],
    presence: [0, 2, 0, 2, 0, 0, 0, 17, 0, 28, 2, 42],
    color: "wine",
    board: { x: 72, y: 28, visible: true },
  },
  {
    id: "alvargio",
    name: "Alvargio de Sotocorvo",
    aliases: ["Maestre", "Sotocorvo"],
    category: "Principal",
    status: "Activo",
    role: "Maestre de la Capilla del Sol y tercer integrante del equipo investigador",
    occupation: "Religioso, alquimista de artilleros durante la guerra y archivista investigador",
    origin: "No precisado",
    affiliation: "Sacro Hierro · Capilla del Sol",
    summary: "Superviviente de la guerra, amigo de Diego y puente entre la investigación, los archivos de la Orden y el pasado de los artilleros.",
    background: "Conoció a Diego en el frente cuando trabajaba protegido por un traje de alquimista. Tras la guerra se convirtió en Maestre de la Capilla del Sol. Fue testigo de la destrucción de los artilleros exigida por los acuerdos de paz, por lo que su reaparición lo golpea de forma personal. Su sensibilidad parece captar voces o preocupaciones que otros no oyen.",
    physical: "Cuerpo marcado por cicatrices. Su vida monástica es austera; en la guerra usaba un pesado traje protector con ribetes dorados.",
    traits: ["Devoto", "Erudito", "Paciente", "Generoso", "Autoindulgente", "Traumatizado"],
    evidence: [
      { chapter: "3", text: "Oye voces sobre ratas y abominaciones antes de ver las sombras del puerto." },
      { chapter: "4", text: "Complementa el informe de Diego y le entrega reportes archivados." },
      { chapter: "11", text: "Convierte a Diego, Alessandra y él mismo en un equipo formal." },
    ],
    presence: [2, 0, 0, 24, 11, 0, 0, 0, 0, 23, 0, 5],
    color: "blue",
    board: { x: 25, y: 27, visible: true },
  },
  {
    id: "jose",
    name: "José Vomero",
    aliases: ["Vomero", "Excelentísimo notario"],
    category: "Principal",
    status: "Activo",
    role: "Amigo íntimo de Diego y testigo del ataque de los gemelos",
    occupation: "Notario",
    origin: "No precisado",
    affiliation: "Círculo de Diego",
    summary: "Notario jovial, glotón y mujeriego; forma con Diego y Lài el núcleo social que ancla la investigación en la vida cotidiana del puerto.",
    background: "Es cliente habitual del Hacha y Carcaj y compañero de naipes de Diego y Lài. Las especias del vino de Lài le calman la cabeza. Los gemelos casi lo atraviesan durante el ataque del capítulo 10.",
    physical: "No descrito con precisión; su expresividad y apetito dominan sus escenas.",
    traits: ["Jovial", "Glotón", "Mujeriego", "Leal", "Despreocupado"],
    evidence: [
      { chapter: "1", text: "Presentado como notario y amigo de Diego en el Hacha y Carcaj." },
      { chapter: "10", text: "Es atacado junto a Diego y Lài por los gemelos." },
    ],
    presence: [0, 13, 5, 0, 2, 0, 0, 0, 0, 0, 17, 1],
    color: "olive",
    board: { x: 20, y: 67, visible: true },
  },
  {
    id: "lai",
    name: "Lài Wēiyuán",
    aliases: ["El delegado", "El agregado", "Wēiyuán"],
    category: "Principal",
    status: "Activo",
    role: "Delegado de Shuzún y amigo íntimo de Diego",
    occupation: "Delegado diplomático",
    origin: "Shuzún",
    affiliation: "Delegación de Shuzún · círculo de Diego",
    summary: "Diplomático de humor seco, amigo del trío y amante secreto de Felisa; tras su muerte sostiene una rutina impecable mientras su duelo amenaza con quebrarlo.",
    background: "Adaptó sus ropas tradicionales al estilo de Thet y conserva vínculos con la Emperatriz. Su forma contenida de bromear contrasta con el caos de José. Durante el ataque de los gemelos, su embriaguez complica la defensa de Diego. El capítulo 12 confirma que mantuvo una relación íntima y secreta con Felisa: conserva contra la piel el guante de encaje que le quitó la primera noche juntos y lucha contra pensamientos autodestructivos después de su muerte.",
    physical: "Ropas holgadas de Shuzún adaptadas a Thet, llenas de bolsillos ocultos.",
    traits: ["Astuto", "Irónico", "Contenido", "Jugador", "Leal"],
    evidence: [
      { chapter: "1", text: "Presentado como delegado de Shuzún y tercer integrante del grupo." },
      { chapter: "10", text: "Atacado por los gemelos; su túnica queda perforada." },
      { chapter: "12", text: "Conserva el guante de encaje de Felisa, recuerda sus visitas y enfrenta en secreto un duelo con pensamientos autodestructivos." },
    ],
    presence: [0, 19, 5, 0, 3, 0, 0, 0, 0, 0, 17, 1, 17],
    color: "jade",
    board: { x: 36, y: 82, visible: true },
  },
  {
    id: "anselmo",
    name: "Anselmo Serdán",
    aliases: ["Capitán Anselmo", "Comandante Serdán"],
    category: "Secundario",
    status: "Activo",
    role: "Comandante de la guardia, padre de Felisa y padre adoptivo de Alessandra",
    occupation: "Comandante de la guardia del puerto",
    origin: "Puerto Ámbar",
    affiliation: "Guardia del puerto · familia Serdán",
    summary: "Mentor de Diego, rescatista de Alessandra y padre quebrado por el asesinato de Felisa.",
    background: "Entrenó a Diego adolescente y soportó su resentimiento por ser el único que volvió cuando murió el padre del muchacho. Veintiún años antes lideró el asalto a la casona malteria, salvó a tres cautivas y llevó a Alessandra a su hogar. Es amigo antiguo del Prior Lorenzo.",
    physical: "En su juventud, rubio y atlético; en el presente ha perdido parte de su agilidad, pero conserva autoridad y fuerza.",
    traits: ["Marcial", "Protector", "Paciente", "Leal", "Devastado"],
    evidence: [
      { chapter: "8", text: "Rescata a las cautivas y se lleva a Alessandra a casa." },
      { chapter: "9", text: "No consigue mirar el cuerpo de Felisa y descarga su dolor golpeando a Diego." },
    ],
    presence: [0, 1, 0, 0, 1, 0, 0, 0, 16, 18, 1, 2],
    color: "steel",
    board: { x: 53, y: 7, visible: true },
  },
  {
    id: "felisa",
    name: "Felisa Serdán",
    aliases: ["Felisa Sartán (error de Amis)"],
    category: "Secundario",
    status: "Muerto",
    role: "Segunda víctima conocida del patrón de extracción de ojos",
    occupation: "No precisada",
    origin: "Puerto Ámbar",
    affiliation: "Familia Serdán",
    summary: "Hija de Anselmo y Yolanda, hermana adoptiva de Alessandra y amante secreta de Lài; su muerte convierte el caso en una herida personal para todo el círculo.",
    background: "Era conocida por su temperamento fuerte. Dos noches antes de morir golpeó con un zapato a un guardia que intentaba terminar la relación. El examen de Diego indica que estuvo con otra persona instantes antes del ataque y que no murió en la plaza donde fue encontrada. El capítulo 12 confirma que mantuvo encuentros íntimos con Lài y que él conserva uno de sus guantes como recuerdo.",
    physical: "La escena conserva una zapatilla de tacón bajo con brillantes. El cadáver aparece sin ojos, pero sin las quemaduras del buwano.",
    traits: ["Directa", "Temperamental", "Decidida", "Reservada en su intimidad"],
    evidence: [
      { chapter: "1", text: "Arroja un zapato al guardia con quien mantenía una relación." },
      { chapter: "9", text: "Diego determina que el cuerpo fue trasladado y que el método difiere del primer crimen." },
      { chapter: "12", text: "El recuerdo de Lài confirma que ambos fueron amantes en secreto y que ella dejó un guante de encaje en su poder." },
    ],
    presence: [0, 1, 0, 0, 0, 0, 0, 1, 0, 14, 2, 2, 6],
    color: "red",
    board: { x: 72, y: 7, visible: true },
  },
  {
    id: "makira",
    name: "Makira al-Dalila",
    aliases: ["Makira"],
    category: "Secundario",
    status: "Activo",
    role: "Joven aljanubarí sometida a una compulsión anómala",
    occupation: "Ayudante temporal en Casa Bajamar",
    origin: "Aljanubar",
    affiliation: "Acogida por Casa Bajamar",
    summary: "Se acerca a Diego después de que alguien le golpea el hombro; el beso no completa lo que debía ocurrir y queda repitiendo «Fallo, error».",
    background: "Conoce a Diego durante el carnaval y lo conduce lejos del gentío. Tras el beso, él siente que un cristal se rompe. Alessandra la encuentra después vagando descalza, sucia y sin propósito, y la acoge en Casa Bajamar.",
    physical: "Piel dorada, ojos verdes almendrados, cabello oscuro y cuerpo menudo.",
    traits: ["Confusa", "Vulnerable", "Directa", "Desarraigada"],
    evidence: [
      { chapter: "2", text: "Besa a Diego; la escena se quiebra de forma súbita e inexplicable." },
      { chapter: "11", text: "Explica que la necesidad apareció tras un golpe en el hombro y acaba en «Fallo, error»." },
    ],
    presence: [0, 0, 6, 0, 0, 0, 0, 1, 0, 0, 0, 4],
    color: "sand",
    board: { x: 90, y: 48, visible: true },
  },
  {
    id: "lorenzo",
    name: "Lorenzo de las Canyas",
    aliases: ["Prior General", "Alto Prior", "Prior Lorenzo"],
    category: "Secundario",
    status: "Activo",
    role: "Máxima autoridad visible del Sacro Hierro en Thet",
    occupation: "Prior General",
    origin: "No precisado",
    affiliation: "Sacro Hierro",
    summary: "Entrega a Diego la Charta de Ferro y convierte los artilleros del puerto en un asunto de paz internacional.",
    background: "Es amigo de Anselmo desde antes de la adopción de Alessandra. Conoce los símbolos y cultos de la casona malteria y advierte que Diego debe ser enviado lejos para protegerlo. Años después le encomienda formalmente la investigación.",
    physical: "No descrito en detalle; su autoridad se expresa por el ceremonial de la Orden.",
    traits: ["Solemne", "Estratégico", "Protector", "Reservado"],
    evidence: [
      { chapter: "4", text: "Explica el riesgo diplomático y entrega la Charta de Ferro." },
      { chapter: "8", text: "Reconoce las hoces y ordena proteger a Diego." },
    ],
    presence: [0, 0, 0, 0, 3, 0, 0, 0, 2, 2, 0, 0],
    color: "violet",
    board: { x: 36, y: 7, visible: false },
  },
  {
    id: "clota",
    name: "Clotilde",
    aliases: ["Clota", "Doña Clota", "Vieja bruja"],
    category: "Secundario",
    status: "Activo",
    role: "Antigua aya de Diego y hospedera del buwano",
    occupation: "Dueña de una hospedería",
    origin: "Puerto Ámbar",
    affiliation: "Hospedería de Clota · entorno Montemar",
    summary: "Criadora de Diego y última persona conocida que alojó al buwano antes de su muerte.",
    background: "Diego vivió con ella en su adolescencia y todavía le obedece a regañadientes. Tras diecisiete años sin verse, la encuentra inconsciente por una caída provocada durante el caos de Chicho y Cucho. Confirma que el buwano llegó al inicio del carnaval, salió esa noche y no volvió.",
    physical: "Bajita, rellenita y canosa; aspecto adorable solo para quien no la conoce.",
    traits: ["Gruñona", "Afectuosa", "Autoritaria", "Resistente", "Práctica"],
    evidence: [
      { chapter: "6", text: "Reconoce la llave del huésped y permite a Diego revisar la habitación." },
      { chapter: "8", text: "Aparece como refugio y figura materna del Diego adolescente." },
    ],
    presence: [1, 0, 0, 0, 0, 1, 15, 1, 5, 0, 0, 1],
    color: "rose",
    board: { x: 7, y: 51, visible: false },
  },
  {
    id: "yolanda",
    name: "Yolanda Serdán",
    aliases: ["Esposa del comandante", "Madre de Felisa"],
    category: "Secundario",
    status: "Activo",
    role: "Madre de Felisa y madre adoptiva de Alessandra",
    occupation: "No precisada",
    origin: "Puerto Ámbar",
    affiliation: "Familia Serdán",
    summary: "Recibe a Alessandra cuando estaba embarazada y, veintiún años después, debe afrontar la muerte de Felisa.",
    background: "Anselmo le entrega a la niña rescatada de la casona con una broma doméstica. El vínculo familiar queda confirmado cuando Alessandra habla de Anselmo y su esposa como sus padres.",
    physical: "No descrita.",
    traits: ["Maternal", "Acogedora", "Resistente"],
    evidence: [
      { chapter: "8", text: "Anselmo le confía a la niña rescatada mientras ella está encinta." },
      { chapter: "9", text: "Su voz llama a Alessandra después del examen de Felisa." },
    ],
    presence: [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 2],
    color: "rose",
    board: { x: 63, y: 4, visible: false },
  },
  {
    id: "buwano",
    name: "El viajero buwano",
    aliases: ["El buwano", "La primera víctima", "Mir Tobe"],
    category: "Secundario",
    status: "Muerto",
    role: "Primera víctima del caso",
    occupation: "Trabajador ligado a contratos comerciales",
    origin: "Buwe",
    affiliation: "Devoto de Lahané",
    summary: "Viajero anónimo que llega buscando libertad y termina muerto, sin ojos y con las cuencas quemadas.",
    background: "Había visitado Puerto Ámbar por trabajo, pero esta vez viajó con sus ahorros. Embarcó en Iskanat en el Sueño de Pawné, se encerró durante la fiesta sexual y se hospedó donde Clota. Su cuerpo aparece la primera noche del carnaval, erguido junto a la costanera.",
    physical: "Viste traje de Buwe con camisa y pantalones rectos, faldilla y sombrero redondo sin alas. La autopsia muestra extracción limpia de ojos y quemaduras con fósforo tratado.",
    traits: ["Devoto", "Recatado", "Trabajador", "Curioso", "Esperanzado"],
    evidence: [
      { chapter: "7 · Interludio", text: "Su propio punto de vista reconstruye el viaje y su llegada al carnaval." },
      { chapter: "5", text: "Diego halla quemaduras, extracción de ojos y la llave de Clota." },
    ],
    presence: [0, 0, 0, 1, 2, 4, 3, 13, 0, 2, 2, 0],
    color: "paper",
    board: { x: 52, y: 84, visible: true },
  },
  {
    id: "gemelos",
    name: "Los gemelos",
    aliases: ["Los idénticos", "Supuestos enviados de Utterdom"],
    category: "Secundario",
    status: "Desconocido",
    role: "Observadores, infiltrados y atacantes de Diego",
    occupation: "Desconocida; afirman pertenecer a una embajada",
    origin: "Desconocido; usan Utterdom como cobertura",
    affiliation: "No identificada",
    summary: "Dos hombres de simetría perfecta que siguen a Diego, inspeccionan sus paradas, atacan al trío y dejan una hoz de oro.",
    background: "Aparecen desde el primer día siguiendo a Diego. Un rondín los ve salir por detrás del pósito y Asmodelus recibe de ellos una piocha de oro. En el ataque se mueven como reflejos, concluyen que Diego «no despertó» y que debieron mandar «una completa, no una media».",
    physical: "Dos siluetas y rostros desconcertantemente idénticos; se mueven como imágenes especulares.",
    traits: ["Sincronizados", "Fríos", "Crípticos", "Letales", "Metódicos"],
    evidence: [
      { chapter: "10", text: "Atacan para verificar algo en Diego y abandonan una hoz dorada." },
      { chapter: "11", text: "Alessandra prueba que recorrieron cada parada de la investigación." },
    ],
    presence: [0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 5, 5],
    color: "black",
    board: { x: 79, y: 74, visible: true },
  },
  {
    id: "wolfrik",
    name: "Wolfrik",
    aliases: ["El felsonio", "Capitán del viaje"],
    category: "Secundario",
    status: "Activo",
    role: "Copropietario y capitán ocasional del Sueño de Pawné",
    occupation: "Marino y comerciante",
    origin: "Felsenthrone; Donnerau",
    affiliation: "Sueño de Pawné",
    summary: "Socio de Taziri y pareja de Koyamï; convirtió por accidente un viaje comercial en fiesta de alta mar.",
    background: "Asume la capitanía durante el trayecto desde Utterdom a Puerto Ámbar. Intenta ocultar a Koyamï que llegó dos días antes y que el negocio de pasajeros se descontroló. Es el padre del bebé que espera Koyamï.",
    physical: "De porte felsonio; tras discutir con Koyamï luce un ojo morado.",
    traits: ["Impulsivo", "Mujeriego", "Trabajador", "Torpe para mentir"],
    evidence: [{ chapter: "7", text: "Entrega a Diego los documentos del barco y recuerda al pasajero buwano." }],
    presence: [0, 0, 0, 0, 0, 0, 0, 20, 0, 0, 0, 3],
    color: "steel",
    board: { x: 88, y: 15, visible: false },
  },
  {
    id: "taziri",
    name: "Taziri",
    aliases: ["El aljanubarí", "Socio de Wolfrik"],
    category: "Secundario",
    status: "Activo",
    role: "Copropietario y compañero de Wolfrik",
    occupation: "Marino y comerciante",
    origin: "Aljanubar",
    affiliation: "Sueño de Pawné",
    summary: "Socio más contenido del barco y amigo de Koyamï desde antes de que ella conociera a Wolfrik.",
    background: "Respeta su compromiso de no involucrarse con pasajeras y trata de contener a Wolfrik. Participa en el negocio que debía transportar mercancía hacia Aljanubar.",
    physical: "No descrito en detalle; de presencia más sobria que el estereotipo aljanubarí.",
    traits: ["Contenido", "Leal", "Prudente", "Conciliador"],
    evidence: [{ chapter: "7", text: "Interrogado por Diego junto a Wolfrik y Koyamï." }],
    presence: [0, 0, 0, 0, 0, 0, 0, 18, 0, 0, 0, 3],
    color: "sand",
    board: { x: 94, y: 30, visible: false },
  },
  {
    id: "koyami",
    name: "Koyamï",
    aliases: ["Dueña del Sueño de Pawné", "La felsonia"],
    category: "Secundario",
    status: "Activo",
    role: "Propietaria del barco y fuente de la pista sobre la uruguita",
    occupation: "Comerciante y armadora",
    origin: "Donnerau, Felsenthrone",
    affiliation: "Sueño de Pawné",
    summary: "Comerciante embarazada que rechaza transportar uruguita y entrega a Alessandra una de las pistas más peligrosas del caso.",
    background: "Llegó por tierra quince días antes para cerrar negocios. Canceló un encargo de cajas al descubrir que contenían uruguita, pues temía que el odio corrompiera a su hijo no nacido. Mantiene una relación turbulenta con Wolfrik y una amistad previa con Taziri.",
    physical: "Mujer fuerte, de embarazo avanzado; conserva agilidad y una pegada demoledora.",
    traits: ["Directa", "Protectora", "Comerciante", "Fuerte", "Informada"],
    evidence: [
      { chapter: "7", text: "Revisa la documentación del barco y encara a sus socios." },
      { chapter: "11", text: "Revela el encargo cancelado de uruguita." },
    ],
    presence: [0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 8],
    color: "blue",
    board: { x: 91, y: 9, visible: false },
  },
  {
    id: "amis",
    name: "Amis",
    aliases: ["La niña", "Pupila de Alessandra"],
    category: "Secundario",
    status: "Activo",
    role: "Joven bajo la tutela de Alessandra y mensajera del hallazgo de Felisa",
    occupation: "Estudiante",
    origin: "Puerto Ámbar",
    affiliation: "Casa Bajamar",
    summary: "Niña curiosa, desobediente y veloz para llevar chismes; anuncia la segunda muerte al equipo.",
    background: "Alessandra protege su educación y disciplina, aunque Amis prefiere escabullirse, comer en la cocina y escuchar todo lo que ocurre en el puerto.",
    physical: "Joven; aparece sucia después de hozar en la tierra.",
    traits: ["Curiosa", "Traviesa", "Deslenguada", "Afectuosa"],
    evidence: [{ chapter: "7", text: "Irrumpe en la cocina y anuncia que apareció una chica muerta sin ojos." }],
    presence: [0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 3],
    color: "yellow",
    board: { x: 95, y: 58, visible: false },
  },
  {
    id: "bram",
    name: "Bram",
    aliases: ["El anciano librero"],
    category: "Secundario",
    status: "Activo",
    role: "Librero de confianza de Alvargio",
    occupation: "Librero y coleccionista",
    origin: "Descendiente de la Orden del Aequitas Electrum",
    affiliation: "Librería de Puerto Ámbar",
    summary: "Vendedor histriónico que consigue para Alvargio libros, documentos y materiales difíciles de hallar.",
    background: "Su familia desciende de miembros de la olvidada Orden del Aequitas Electrum. Alvargio le entrega una moneda auténtica como regalo después de completar la investigación necesaria.",
    physical: "Bajo, regordete, cejas gruesas, cabello revuelto y pinzares; se mueve por escaleras con agilidad infantil.",
    traits: ["Histriónico", "Ágil", "Afable", "Perspicaz", "Coleccionista"],
    evidence: [{ chapter: "3", text: "Entrega a Alvargio el Tractat de les quatre mocions del cor de l’home." }],
    presence: [0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0],
    color: "olive",
    board: { x: 11, y: 37, visible: false },
  },
  {
    id: "jacobo",
    name: "Jacobo",
    aliases: ["Dueño del Hacha y Carcaj"],
    category: "Secundario",
    status: "Activo",
    role: "Tabernero y custodio involuntario de una hoz dorada",
    occupation: "Dueño del Hacha y Carcaj",
    origin: "Puerto Ámbar",
    affiliation: "Hacha y Carcaj",
    summary: "Amigo del trío y testigo periférico del ataque; recoge como propina la insignia que dejan los gemelos.",
    background: "Su familia atiende el bar desde generaciones. Impone una regla de no hablar de trabajo en el jardín interior y mantiene a Diego, José y Lài alimentados, endeudados y relativamente cuerdos.",
    physical: "No descrito en detalle.",
    traits: ["Práctico", "Hospitalario", "Observador", "Comerciante"],
    evidence: [{ chapter: "10", text: "Encuentra una pequeña insignia de oro con forma de hoz y se la guarda." }],
    presence: [0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 11, 0],
    color: "amber",
    board: { x: 12, y: 79, visible: false },
  },
  {
    id: "galdones",
    name: "Intendente Galdones",
    aliases: ["Inte Gardóne"],
    category: "Incidental",
    status: "Activo",
    role: "Responsable del Pósito del Muelle",
    occupation: "Intendente",
    origin: "Puerto Ámbar",
    affiliation: "Pósito del Muelle",
    summary: "Facilita a Diego la inspección del primer cadáver y dirige a Asmodelus.",
    background: "Recibe al investigador con una reverencia exagerada y moviliza a estibadores y ayudantes durante el examen forense.",
    physical: "Rechoncho, casi un codo más bajo que Diego, cachetes rosados y rostro curtido por el viento.",
    traits: ["Servicial", "Formal", "Nervioso"],
    evidence: [{ chapter: "5", text: "Abre la cava y presencia el examen de las cuencas quemadas." }],
    presence: [0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0],
    color: "paper",
    board: { x: 62, y: 93, visible: false },
  },
  {
    id: "asmodelus",
    name: "Asmodelus",
    aliases: ["Ratón", "El portero del pósito"],
    category: "Secundario",
    status: "Activo",
    role: "Testigo oculto de los gemelos",
    occupation: "Portero y anotador de carga en la lonja",
    origin: "Puerto Ámbar",
    affiliation: "Pósito del Muelle",
    summary: "Empleado escurridizo que vio a los gemelos salir del pósito y recibió de ellos una piocha dorada.",
    background: "Desconfía de Diego por haberse llevado a un amigo suyo. Oculta información durante la primera visita y solo la revela cuando Alessandra lo somete a presión.",
    physical: "Extremadamente delgado, rostro huesudo, ojos grandes hundidos y bigote largo y ralo.",
    traits: ["Nervioso", "Escurridizo", "Codicioso", "Observador"],
    evidence: [{ chapter: "11", text: "Confiesa que un rondín vio a dos idénticos salir por detrás del pósito." }],
    presence: [0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 8],
    color: "paper",
    board: { x: 67, y: 92, visible: false },
  },
  {
    id: "jorge",
    name: "Jorge",
    aliases: ["Hombre de confianza de la Signora"],
    category: "Incidental",
    status: "Activo",
    role: "Encargado de caballos e informante logístico de Alessandra",
    occupation: "Caballerizo y hombre de recursos",
    origin: "Puerto Ámbar",
    affiliation: "Casa Bajamar",
    summary: "Consigue el itinerario completo de Diego y advierte que alguien sigue al investigador.",
    background: "Guarda los caballos, gestiona compras y reúne discretamente información útil para la Signora.",
    physical: "No descrito.",
    traits: ["Discreto", "Leal", "Eficaz"],
    evidence: [{ chapter: "11", text: "Entrega a Alessandra el itinerario del investigador y una advertencia." }],
    presence: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4],
    color: "olive",
    board: { x: 97, y: 83, visible: false },
  },
  {
    id: "murmuradora",
    name: "Murmuradora",
    aliases: ["Artillero cautivo"],
    category: "Secundario",
    status: "Desconocido",
    role: "Artillero vinculado a la infancia cautiva de Alessandra",
    occupation: "Víctima convertida en artillero",
    origin: "Desconocido",
    affiliation: "Cautivo de la congregación malteria",
    summary: "Uno de los hombres pálidos y deformados que acompaña y protege emocionalmente a la niña vendada.",
    background: "Camina siempre delante de la niña durante su cautiverio. Está amordazado, obligado a andar a cuatro patas y recibe de ella una caricia antes de la separación.",
    physical: "Muy delgado, ojeras profundas, ojos oscuros, labios negros e incisivos anchos y astillados.",
    traits: ["Leal", "Deformado", "Silencioso", "Protector"],
    evidence: [{ chapter: "8", text: "Es revelado como hombre amordazado después de ser percibido como compañera cautiva." }],
    presence: [0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0],
    color: "black",
    board: { x: 8, y: 12, visible: false },
  },
  {
    id: "chasqueadora",
    name: "Chasqueadora",
    aliases: ["Artillero cautivo"],
    category: "Secundario",
    status: "Desconocido",
    role: "Artillero vinculado a la infancia cautiva de Alessandra",
    occupation: "Víctima convertida en artillero",
    origin: "Desconocido",
    affiliation: "Cautivo de la congregación malteria",
    summary: "Segundo artillero de la pequeña familia que la niña crea durante su encierro.",
    background: "Camina detrás de la niña y, como Murmuradora, recibe una caricia antes de ser apartado a golpes.",
    physical: "Comparte la extrema delgadez, ojos vacíos, ojeras y dentadura rota de los artilleros.",
    traits: ["Leal", "Deformado", "Silencioso", "Protector"],
    evidence: [{ chapter: "8", text: "Viaja hacia Puerto Ámbar en uno de los carros cubiertos." }],
    presence: [0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0],
    color: "black",
    board: { x: 14, y: 20, visible: false },
  },
  {
    id: "ortia",
    name: "Ortía",
    aliases: ["La dama alta", "La señora de negro"],
    category: "Secundario",
    status: "Desconocido",
    role: "Captora y preparadora de la niña destinada al Astronomago",
    occupation: "Comadrona o adepta de alto rango",
    origin: "Vinculada a Malteria",
    affiliation: "Congregación de Raceta",
    summary: "Supervisa el engorde, traslado, ornamentación y posible sacrificio de Alessandra.",
    background: "Exige que la niña llegue presentable para satisfacer a Raceta y al Astronomago. Durante la redada intenta abrirle el vientre y Anselmo la desarma con un estilete.",
    physical: "Muy alta y espigada; viste de negro impecable del cuello a los pies.",
    traits: ["Cruel", "Controladora", "Ritualista", "Imperturbable"],
    evidence: [{ chapter: "8", text: "Le susurra a la niña «Serás una delicia» antes de conducirla al rito." }],
    presence: [0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0],
    color: "black",
    board: { x: 18, y: 6, visible: false },
  },
  {
    id: "astronomago",
    name: "El Astronomago",
    aliases: ["Líder de la congregación en Thet", "Alto sacerdote"],
    category: "Secundario",
    status: "Desconocido",
    role: "Oficiante del sacrificio de las jóvenes cautivas",
    occupation: "Líder de culto",
    origin: "Desconocido",
    affiliation: "Congregación de Raceta el Celoso",
    summary: "Sacerdote que usa hoces doradas para abrir a las cautivas y retirar sus órganos.",
    background: "Llega a la casona del cerro Bajamar para la primera noche de primavera. Dispone doce lechos de piedra y doce hoces; alcanza a intervenir a nueve niñas antes de la irrupción de la guardia.",
    physical: "Túnica púrpura y oro cubierta de símbolos; mano nudosa.",
    traits: ["Ritualista", "Cruel", "Metódico", "Fanático"],
    evidence: [{ chapter: "8", text: "Ejecuta el rito mientras Diego resiste al otro lado de las puertas." }],
    presence: [0, 0, 0, 0, 0, 0, 0, 0, 12, 0, 0, 0],
    color: "violet",
    board: { x: 29, y: 8, visible: false },
  },
  {
    id: "raceta",
    name: "Raceta el Celoso",
    aliases: ["Nuestro señor", "L’Exultarie"],
    category: "Entidad",
    status: "Entidad",
    role: "Figura venerada por la congregación",
    occupation: "Deidad o señor del culto",
    origin: "Cosmología de la congregación",
    affiliation: "Culto malterio",
    summary: "Nombre bajo el cual la congregación justifica el traslado y sacrificio de las jóvenes.",
    background: "Su naturaleza real no está explicada en el manuscrito. La procesión interpreta la estrella Exultante como señal de su favor.",
    physical: "Sin manifestación física conocida.",
    traits: ["Celoso", "Punitivo", "Incierto"],
    evidence: [{ chapter: "8", text: "Ortía afirma que la víctima debe satisfacerlo y que su luz bendice la llegada." }],
    presence: [0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0],
    color: "violet",
    board: { x: 34, y: 2, visible: false },
  },
  {
    id: "thul",
    name: "Thul",
    aliases: ["Asistente de Alvargio"],
    category: "Incidental",
    status: "Activo",
    role: "Ayudante o novicio de la Capilla del Sol",
    occupation: "Novicio",
    origin: "No precisado",
    affiliation: "Sacro Hierro",
    summary: "Nombre al que Alvargio intenta dictar una ocurrencia antes de recordar que envió a su asistente de vuelta.",
    background: "Ayuda a Alvargio a vestirse y gestionar manuscritos, aunque el texto todavía no lo presenta de forma directa por nombre y función en la misma frase.",
    physical: "Joven novicio; sin más descripción.",
    traits: ["Servicial"],
    evidence: [{ chapter: "3", text: "Alvargio empieza a decir «anótalo, Thul…» y recuerda que está solo." }],
    presence: [0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0],
    color: "blue",
    board: { x: 15, y: 29, visible: false },
  },
  {
    id: "ukiel",
    name: "Ukiel",
    aliases: ["El veterano"],
    category: "Incidental",
    status: "Desconocido",
    role: "Veterano atacado por figuras semejantes a artilleros",
    occupation: "Veterano de guerra",
    origin: "Puerto Ámbar",
    affiliation: "Antiguo compañero de armas de Diego",
    summary: "Ejemplo visible del deterioro de los veteranos y una pista temprana de que las abominaciones ya actúan en el puerto.",
    background: "Los rumores dicen que fue atacado y no pudo defenderse por sus espasmos. Diego lo recuerda con compasión al pensar en los compañeros que no se recuperaron de la guerra.",
    physical: "No descrito.",
    traits: ["Traumatizado", "Vulnerable"],
    evidence: [{ chapter: "3", text: "Una voz informa que las criaturas atacaron a Ukiel." }],
    presence: [0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0],
    color: "steel",
    board: { x: 8, y: 72, visible: false },
  },
  {
    id: "virggio",
    name: "Virggio",
    aliases: ["El alcohólico con ínfulas de poeta"],
    category: "Incidental",
    status: "Activo",
    role: "Figura cómica habitual del puerto",
    occupation: "Poeta aficionado",
    origin: "Puerto Ámbar",
    affiliation: "Ambiente del Hacha y Carcaj",
    summary: "Poeta borracho conocido por montar espectáculos involuntarios, como recitarle a un burro.",
    background: "Sirve como referencia común en las bromas del grupo y da textura a la vida cotidiana de Puerto Ámbar.",
    physical: "No descrito.",
    traits: ["Alcohólico", "Histriónico", "Poeta"],
    evidence: [{ chapter: "1", text: "Lài cuenta que le recitó sus mejores versos a un burro." }],
    presence: [0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    color: "amber",
    board: { x: 6, y: 85, visible: false },
  },
  {
    id: "athelfmain",
    name: "Athelfmain Noiktann",
    aliases: ["Primer capitán del Sueño de Pawné"],
    category: "Histórico",
    status: "Histórico",
    role: "Capitán legendario y posible pirata",
    occupation: "Capitán de la armada de Utterdom",
    origin: "Utterdom",
    affiliation: "Sueño de Pawné · armada de Utterdom",
    summary: "Capitán asociado a la primera llegada del barco a las Islas Invisibles y a versiones contradictorias sobre su final.",
    background: "La leyenda dice que desapareció con toda su tripulación al desembarcar; otra versión afirma que fue denunciado y ejecutado por piratería no autorizada.",
    physical: "No descrito.",
    traits: ["Legendario", "Temerario", "Ambiguo"],
    evidence: [{ chapter: "7", text: "El charlatán narra su historia al forastero frente al barco." }],
    presence: [0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0],
    color: "paper",
    board: { x: 86, y: 91, visible: false },
  },
  {
    id: "isoalda",
    name: "Isoalda Vermún",
    aliases: ["Capitana del Sueño de Pawné"],
    category: "Histórico",
    status: "Histórico",
    role: "Capitana pirata del barco",
    occupation: "Capitana",
    origin: "No precisado",
    affiliation: "Sueño de Pawné",
    summary: "Capitana que aterrorizó puertos del norte y oyó un ejército imposible cerca de las Islas Invisibles.",
    background: "Usó el barco para el pillaje en Utterdom, Felsenthrone y el norte de Aljanubar. Prefirió no desembarcar en las Islas Invisibles tras oír millares de carabinas manipuladas en la oscuridad.",
    physical: "No descrita.",
    traits: ["Pirata", "Prudente ante lo anómalo", "Audaz"],
    evidence: [{ chapter: "7", text: "Segunda gran historia del Sueño de Pawné narrada por el charlatán." }],
    presence: [0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0],
    color: "paper",
    board: { x: 95, y: 92, visible: false },
  },
  {
    id: "joao",
    name: "Joao Stuardo",
    aliases: ["El Libertador"],
    category: "Histórico",
    status: "Histórico",
    role: "Figura histórica que da nombre a una avenida",
    occupation: "No precisada",
    origin: "No precisado",
    affiliation: "Memoria pública de Puerto Ámbar",
    summary: "Personaje histórico aún no desarrollado, preservado en el nombre de la avenida Libertador Joao Stuardo.",
    background: "El manuscrito no entrega todavía otros antecedentes.",
    physical: "No descrito.",
    traits: ["Figura conmemorativa"],
    evidence: [{ chapter: "2", text: "La avenida que conduce a Santa Gracia lleva su nombre." }],
    presence: [0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    color: "steel",
    board: { x: 3, y: 94, visible: false },
  },
  {
    id: "chicho",
    name: "Chicho",
    aliases: ["El mapache", "Alimaña de Clota"],
    category: "Incidental",
    status: "Activo",
    role: "Mapache criado alrededor de la hospedería",
    occupation: "Saqueador de despensas",
    origin: "Puerto Ámbar",
    affiliation: "Hospedería de Clota",
    summary: "Mapache manipulador y compañero de infancia de Diego; junto con Cucho provoca el accidente de Clota.",
    background: "Diego lo alimentaba incluso cuando Clota intentaba castigarlo. Veintiún años después sigue causando destrozos en la casona.",
    physical: "Bola de pelos de mirada capaz de partir el alma.",
    traits: ["Glotón", "Manipulador", "Travieso"],
    evidence: [{ chapter: "6", text: "Clota lo culpa junto a Cucho del desastre de la casa." }],
    presence: [0, 0, 0, 0, 0, 0, 1, 0, 4, 0, 0, 0],
    color: "paper",
    board: { x: 3, y: 58, visible: false },
  },
  {
    id: "cucho",
    name: "Cucho",
    aliases: ["El gato del vecino"],
    category: "Incidental",
    status: "Activo",
    role: "Gato asociado a Clota y Chicho",
    occupation: "Gato",
    origin: "Puerto Ámbar",
    affiliation: "Ño Galao · hospedería de Clota",
    summary: "Gato que participa con Chicho en los juegos y destrozos de la cocina.",
    background: "Era una cría durante la adolescencia de Diego. Su nombre encabeza junto a Clota el capítulo del reencuentro.",
    physical: "No descrito más allá de su condición de gato.",
    traits: ["Travieso", "Persistente"],
    evidence: [{ chapter: "6", text: "Clota atribuye a Chicho y Cucho la caída que la deja inconsciente." }],
    presence: [0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 0, 0],
    color: "paper",
    board: { x: 4, y: 63, visible: false },
  },
  {
    id: "galao",
    name: "Ño Galao",
    aliases: ["El vecino"],
    category: "Incidental",
    status: "Activo",
    role: "Dueño nominal de Cucho",
    occupation: "No precisada",
    origin: "Puerto Ámbar",
    affiliation: "Vecindario de Clota",
    summary: "Vecino al que Diego propone cobrar pensión por la presencia constante de su gato.",
    background: "El manuscrito solo lo menciona en la escena de la adolescencia de Diego.",
    physical: "No descrito.",
    traits: ["Vecino ausente"],
    evidence: [{ chapter: "8", text: "Clota y Diego discuten sobre cobrarle por Cucho." }],
    presence: [0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0],
    color: "paper",
    board: { x: 2, y: 68, visible: false },
  },
  {
    id: "guardia-felisa",
    name: "El guardia de Felisa",
    aliases: ["Pretendiente de Felisa", "El unicornio"],
    category: "Incidental",
    status: "Activo",
    role: "Pareja que intentó terminar con Felisa",
    occupation: "Guardia del puerto",
    origin: "Puerto Ámbar",
    affiliation: "Guardia del puerto",
    summary: "Testigo íntimo de Felisa que confirma que ella pudo estar viendo a otra persona antes de morir.",
    background: "Intenta terminar la relación porque Felisa lo obligaba a vestirse de unicornio cuando estaban solos. Recibe un zapatazo y queda con un moretón visible.",
    physical: "Moretón en el mentón tras el zapatazo.",
    traits: ["Avergonzado", "Honesto bajo presión", "Temeroso de Alessandra"],
    evidence: [{ chapter: "9", text: "Confiesa la relación y explica el altercado de la primera noche." }],
    presence: [0, 4, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0],
    color: "steel",
    board: { x: 79, y: 4, visible: false },
  },
  {
    id: "charlatan",
    name: "El charlatán del tronco",
    aliases: ["El anciano", "El ebrio", "Narrador del puerto"],
    category: "Incidental",
    status: "Activo",
    role: "Narrador intradiegético y vendedor de historias",
    occupation: "Charlatán, guía informal y bebedor profesional",
    origin: "Puerto Ámbar",
    affiliation: "Calles del puerto",
    summary: "Interpela a forasteros, presenta a Diego y cuenta la historia del Sueño de Pawné a cambio de monedas.",
    background: "Parece haber conocido tiempos mejores. Conoce a los habitantes, rumores e historias del puerto y usa al lector-forastero como audiencia.",
    physical: "Anciano de nariz grande y roja, mejillas caídas, poco cabello revuelto y equilibrio de bebedor experto.",
    traits: ["Histriónico", "Astuto", "Embaucador", "Buen narrador"],
    evidence: [
      { chapter: "1", text: "Presenta Puerto Ámbar y la biografía pública de Diego." },
      { chapter: "7", text: "Narra tres siglos de historias del Sueño de Pawné." },
    ],
    presence: [0, 17, 0, 0, 0, 0, 0, 12, 0, 0, 0, 0],
    color: "amber",
    board: { x: 5, y: 88, visible: false },
  },
  {
    id: "compinches-diego",
    name: "Los compinches de Diego",
    aliases: ["El pelirrojo rollizo", "El granuja delgado"],
    category: "Incidental",
    status: "Desconocido",
    role: "Amigos de adolescencia y cómplices del asalto accidental a la casona",
    occupation: "Estudiantes y granujas",
    origin: "Puerto Ámbar",
    affiliation: "Pandilla juvenil de Diego",
    summary: "Dos muchachos que siguen a Diego a robar alcohol y un jamón, detonando sin saberlo la redada que salva a tres cautivas.",
    background: "Uno es pelirrojo, pecoso y rollizo; el otro más delgado y hermano menor de un guardia. Diego los arroja por una ventana para salvarlos mientras él contiene a los adeptos.",
    physical: "Uno pelirrojo, rollizo y pecoso; el otro delgado. Ambos adolescentes.",
    traits: ["Leales", "Traviesos", "Valientes", "Ladrones improvisados"],
    evidence: [{ chapter: "8", text: "Su incursión por alcohol expone el sacrificio y permite la llegada de la guardia." }],
    presence: [0, 0, 0, 0, 0, 0, 0, 0, 18, 0, 0, 0],
    color: "yellow",
    board: { x: 25, y: 92, visible: false },
  },
  {
    id: "jovenes-cautivas",
    name: "Las jóvenes cautivas",
    aliases: ["Las ofrendas", "Las niñas vendadas"],
    category: "Incidental",
    status: "Desconocido",
    role: "Víctimas colectivas del rito del Astronomago",
    occupation: "Cautivas",
    origin: "Diversos lugares no precisados",
    affiliation: "Prisioneras de la congregación",
    summary: "Grupo de niñas y jóvenes mantenidas drogadas, vendadas y en silencio antes del sacrificio.",
    background: "Doce lechos están preparados. El Astronomago alcanza a intervenir a varias; Anselmo cubre y evacua a tres sobrevivientes. Alessandra es la única cuya continuidad queda trazada en el presente.",
    physical: "Desnutridas al llegar; luego engordadas, adornadas con joyas y gasas para el rito.",
    traits: ["Víctimas", "Silenciadas", "Supervivientes parciales"],
    evidence: [{ chapter: "8", text: "El rito revela extracción de órganos con hoces doradas." }],
    presence: [0, 0, 0, 0, 0, 0, 0, 0, 22, 0, 0, 0],
    color: "rose",
    board: { x: 42, y: 2, visible: false },
  },
  {
    id: "prima-jacobo",
    name: "La prima de Jacobo",
    aliases: ["La jugadora"],
    category: "Incidental",
    status: "Activo",
    role: "Jugadora que despluma al trío",
    occupation: "No precisada",
    origin: "Puerto Ámbar",
    affiliation: "Hacha y Carcaj",
    summary: "Pariente de Jacobo que usa su habilidad y el efecto que causa en los tres amigos para ganarles a los naipes.",
    background: "Su intervención precede el ataque de los gemelos y refuerza la rutina cómica del trío.",
    physical: "Se destacan sus cualidades físicas, sin descripción concreta.",
    traits: ["Hábil", "Segura", "Oportunista"],
    evidence: [{ chapter: "10", text: "Se retira con los bolsillos llenos después de ganarles durante horas." }],
    presence: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0],
    color: "rose",
    board: { x: 13, y: 86, visible: false },
  },
  {
    id: "moza-bajamar",
    name: "La moza de la bandeja",
    aliases: ["Chica de Casa Bajamar"],
    category: "Incidental",
    status: "Activo",
    role: "Trabajadora que introduce la influencia cotidiana de la Signora",
    occupation: "Moza de Casa Bajamar",
    origin: "Puerto Ámbar",
    affiliation: "Casa Bajamar",
    summary: "La primera trabajadora de la casona que aparece: abofetea a Diego después de que este roba una brocheta.",
    background: "Teme que Alessandra le cobre a ella la travesura de Diego y deja claro que el hidalgo es un cliente conocido, no un extraño.",
    physical: "Cabello rizado indomable, largas botas y vestido húmedo por el trabajo.",
    traits: ["Furibunda", "Ágil", "Responsable"],
    evidence: [{ chapter: "1", text: "Le advierte a Diego que la Signora se lo cobrará." }],
    presence: [0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    color: "rose",
    board: { x: 93, y: 62, visible: false },
  },
];

const relationships: RelationshipRecord[] = [
  { id: "r-diego-alessandra", source: "diego", target: "alessandra", type: "Romance", label: "Intimidad, tensión y alianza", certainty: "Hecho", strength: 3, details: "Se desean, se desafían y recurren el uno al otro. El caso los obliga a investigar juntos." },
  { id: "r-diego-alvargio", source: "diego", target: "alvargio", type: "Amistad", label: "Amigos desde la guerra", certainty: "Hecho", strength: 3, details: "Se conocen bajo los cañones malterios; Alvargio ayuda a encauzar a Diego hacia la Orden." },
  { id: "r-diego-jose", source: "diego", target: "jose", type: "Amistad", label: "Amigos y compañeros de naipes", certainty: "Hecho", strength: 3, details: "José integra el núcleo de confianza y casi muere por acompañar a Diego." },
  { id: "r-diego-lai", source: "diego", target: "lai", type: "Amistad", label: "Amigos y compañeros de naipes", certainty: "Hecho", strength: 3, details: "Lài bromea con Diego, lo acompaña y queda expuesto durante el ataque." },
  { id: "r-jose-lai", source: "jose", target: "lai", type: "Amistad", label: "Cómplices habituales", certainty: "Hecho", strength: 2, details: "Comparten vino, apuestas, bromas y desventuras en el carnaval." },
  { id: "r-lai-felisa", source: "lai", target: "felisa", type: "Romance", label: "Amantes secretos", certainty: "Hecho", strength: 3, details: "El capítulo 12 confirma que Felisa visitaba en secreto la habitación de Lài. Él conserva entre la piel y la ropa el guante de encaje que le quitó la primera noche juntos y atraviesa un duelo que oculta bajo su rutina diplomática." },
  { id: "r-diego-anselmo", source: "diego", target: "anselmo", type: "Tutela", label: "Mentor, rival y figura paterna", certainty: "Hecho", strength: 3, details: "Anselmo entrena al Diego adolescente y lo protege incluso cuando recibe su resentimiento." },
  { id: "r-diego-clota", source: "diego", target: "clota", type: "Tutela", label: "Aya y madre práctica", certainty: "Hecho", strength: 3, details: "Clota lo cría, alimenta y disciplina; Diego vuelve a obedecerla incluso de adulto." },
  { id: "r-diego-makira", source: "diego", target: "makira", type: "Romance", label: "Atracción inducida y beso fallido", certainty: "Hecho", strength: 2, details: "El contacto culmina en una ruptura perceptiva que deja a Makira sin propósito." },
  { id: "r-diego-gemelos", source: "diego", target: "gemelos", type: "Conflicto", label: "Vigilado, atacado y evaluado", certainty: "Hecho", strength: 3, details: "Los gemelos recorren sus pasos y lo atacan para comprobar si ha «despertado»." },
  { id: "r-diego-buwano", source: "diego", target: "buwano", type: "Investigación", label: "Primera víctima investigada", certainty: "Hecho", strength: 3, details: "El cuerpo conduce a Diego desde el pósito hasta Clota y el Sueño de Pawné." },
  { id: "r-diego-felisa", source: "diego", target: "felisa", type: "Investigación", label: "Segunda víctima y vínculo familiar", certainty: "Hecho", strength: 3, details: "Conocía a Felisa y debe examinar su cadáver delante de Alessandra." },
  { id: "r-diego-lorenzo", source: "diego", target: "lorenzo", type: "Trabajo", label: "Investigador y mandante", certainty: "Hecho", strength: 2, details: "El Prior le entrega la Charta de Ferro y la responsabilidad diplomática del caso." },
  { id: "r-alessandra-felisa", source: "alessandra", target: "felisa", type: "Familia", label: "Hermanas", certainty: "Hecho", strength: 3, details: "Felisa fue el refugio emocional de Alessandra desde su adopción." },
  { id: "r-alessandra-anselmo", source: "alessandra", target: "anselmo", type: "Familia", label: "Padre adoptivo", certainty: "Hecho", strength: 3, details: "Anselmo la rescata del rito y la cría como hija." },
  { id: "r-alessandra-yolanda", source: "alessandra", target: "yolanda", type: "Familia", label: "Madre adoptiva", certainty: "Hecho", strength: 3, details: "Yolanda recibe a la niña rescatada y forma con ella una familia." },
  { id: "r-anselmo-felisa", source: "anselmo", target: "felisa", type: "Familia", label: "Padre e hija", certainty: "Hecho", strength: 3, details: "La muerte de Felisa descompone la autoridad habitual del comandante." },
  { id: "r-yolanda-felisa", source: "yolanda", target: "felisa", type: "Familia", label: "Madre e hija", certainty: "Hecho", strength: 3, details: "Yolanda espera el cuerpo y comparte el duelo con Alessandra." },
  { id: "r-anselmo-yolanda", source: "anselmo", target: "yolanda", type: "Familia", label: "Matrimonio", certainty: "Hecho", strength: 3, details: "Anselmo vuelve junto a su esposa después del hallazgo de Felisa." },
  { id: "r-anselmo-lorenzo", source: "anselmo", target: "lorenzo", type: "Amistad", label: "Amigos de antigua data", certainty: "Hecho", strength: 2, details: "Su amistad precede la adopción de Alessandra y sostiene la confianza entre Orden y guardia." },
  { id: "r-alessandra-alvargio", source: "alessandra", target: "alvargio", type: "Alianza", label: "Equipo investigador", certainty: "Hecho", strength: 2, details: "Alvargio formaliza la cooperación de los tres al cierre del capítulo 11." },
  { id: "r-alessandra-makira", source: "alessandra", target: "makira", type: "Tutela", label: "La acoge en Casa Bajamar", certainty: "Hecho", strength: 2, details: "Alessandra la encuentra perdida, la protege y le asigna un lugar en la cocina." },
  { id: "r-alessandra-amis", source: "alessandra", target: "amis", type: "Tutela", label: "Protección y disciplina", certainty: "Hecho", strength: 2, details: "Cuida su educación, sus horarios y hasta cómo duerme." },
  { id: "r-alessandra-jorge", source: "alessandra", target: "jorge", type: "Trabajo", label: "Hombre de confianza", certainty: "Hecho", strength: 2, details: "Jorge obtiene información, cuida los caballos y administra tareas discretas." },
  { id: "r-alessandra-asmodelus", source: "alessandra", target: "asmodelus", type: "Investigación", label: "Interrogatorio coercitivo", certainty: "Hecho", strength: 2, details: "Lo obliga a mirar y obtiene la pista que ocultó a Diego." },
  { id: "r-alessandra-koyami", source: "alessandra", target: "koyami", type: "Alianza", label: "Intercambio de información", certainty: "Hecho", strength: 2, details: "Koyamï le cuenta el encargo de uruguita y recibe de ella una confianza inusual." },
  { id: "r-felisa-guardia", source: "felisa", target: "guardia-felisa", type: "Romance", label: "Relación terminada con violencia", certainty: "Hecho", strength: 2, details: "Él intenta terminar; ella responde con un zapato. El motivo declarado es el disfraz de unicornio." },
  { id: "r-buwano-clota", source: "buwano", target: "clota", type: "Trabajo", label: "Huésped y hospedera", certainty: "Hecho", strength: 2, details: "La llave encontrada en el cadáver corresponde a la habitación de Clota." },
  { id: "r-buwano-wolfrik", source: "buwano", target: "wolfrik", type: "Trabajo", label: "Pasajero y capitán", certainty: "Hecho", strength: 2, details: "Wolfrik registra su embarque en Iskanat y su desembarco apresurado." },
  { id: "r-buwano-taziri", source: "buwano", target: "taziri", type: "Trabajo", label: "Pasajero y socio del barco", certainty: "Hecho", strength: 1, details: "Taziri participa en el viaje y en la venta del pasaje." },
  { id: "r-wolfrik-taziri", source: "wolfrik", target: "taziri", type: "Amistad", label: "Socios y amigos", certainty: "Hecho", strength: 3, details: "Compran y operan juntos el Sueño de Pawné." },
  { id: "r-wolfrik-koyami", source: "wolfrik", target: "koyami", type: "Romance", label: "Pareja; esperan un hijo", certainty: "Hecho", strength: 3, details: "La relación no se presenta como oficial, pero el embarazo y los celos la vuelven inequívoca." },
  { id: "r-taziri-koyami", source: "taziri", target: "koyami", type: "Amistad", label: "Amistad anterior a Wolfrik", certainty: "Hecho", strength: 2, details: "Koyamï era amiga de Taziri antes de conocer a Wolfrik." },
  { id: "r-clota-chicho", source: "clota", target: "chicho", type: "Tutela", label: "Cuidadora y alimaña", certainty: "Hecho", strength: 2, details: "Clota intenta disciplinarlo y él vuelve a la despensa." },
  { id: "r-clota-cucho", source: "clota", target: "cucho", type: "Tutela", label: "Cuidadora involuntaria", certainty: "Hecho", strength: 1, details: "El gato del vecino pasa suficiente tiempo en la hospedería para ser parte de la casa." },
  { id: "r-chicho-cucho", source: "chicho", target: "cucho", type: "Amistad", label: "Socios del desastre", certainty: "Hecho", strength: 2, details: "Sus juegos dejan la casa patas arriba y a Clota inconsciente." },
  { id: "r-cucho-galao", source: "cucho", target: "galao", type: "Tutela", label: "Dueño nominal", certainty: "Hecho", strength: 1, details: "Diego propone cobrarle pensión por el gato." },
  { id: "r-murmuradora-chasqueadora", source: "murmuradora", target: "chasqueadora", type: "Alianza", label: "Compañeros inseparables", certainty: "Hecho", strength: 3, details: "Forman con la niña una familia silenciosa durante el cautiverio." },
  { id: "r-alessandra-murmuradora", source: "alessandra", target: "murmuradora", type: "Amistad", label: "Familia del cautiverio", certainty: "Hipótesis", strength: 3, details: "La continuidad entre la niña y Alessandra es muy fuerte, pero el capítulo 8 no pronuncia su nombre." },
  { id: "r-alessandra-chasqueadora", source: "alessandra", target: "chasqueadora", type: "Amistad", label: "Familia del cautiverio", certainty: "Hipótesis", strength: 3, details: "La niña acaricia a ambos antes de ser trasladada al rito." },
  { id: "r-ortia-alessandra", source: "ortia", target: "alessandra", type: "Conflicto", label: "Captora y víctima", certainty: "Hipótesis", strength: 3, details: "La identificación de la niña con Alessandra une las escenas separadas por veintiún años." },
  { id: "r-astronomago-alessandra", source: "astronomago", target: "alessandra", type: "Conflicto", label: "Sacrificador y víctima", certainty: "Hipótesis", strength: 3, details: "La niña de Ortía ocupa el lecho prominente y sobrevive gracias a la irrupción." },
  { id: "r-ortia-astronomago", source: "ortia", target: "astronomago", type: "Culto", label: "Adepta y líder ritual", certainty: "Hecho", strength: 2, details: "Ortía prepara la ofrenda favorita y la conduce al Astronomago." },
  { id: "r-astronomago-raceta", source: "astronomago", target: "raceta", type: "Culto", label: "Oficiante y señor venerado", certainty: "Hecho", strength: 3, details: "El sacrificio se ejecuta para obtener el favor de Raceta." },
  { id: "r-gemelos-buwano", source: "gemelos", target: "buwano", type: "Sospecha", label: "Revisaron el pósito", certainty: "Hipótesis", strength: 2, details: "Un rondín los ve salir por detrás; no está probado que intervinieran el cadáver." },
  { id: "r-gemelos-jacobo", source: "gemelos", target: "jacobo", type: "Investigación", label: "Hoz dorada abandonada", certainty: "Hecho", strength: 2, details: "Jacobo recoge la insignia caída después del ataque." },
  { id: "r-gemelos-koyami", source: "gemelos", target: "koyami", type: "Sospecha", label: "Supuestos enviados de Utterdom", certainty: "Hecho", strength: 1, details: "Se presentan ante ella con una identidad diplomática que el equipo considera improbable." },
  { id: "r-gemelos-asmodelus", source: "gemelos", target: "asmodelus", type: "Sospecha", label: "Piocha dorada", certainty: "Hecho", strength: 2, details: "Asmodelus afirma que recibió de ellos el objeto de oro." },
  { id: "r-galdones-asmodelus", source: "galdones", target: "asmodelus", type: "Trabajo", label: "Superior y portero", certainty: "Hecho", strength: 1, details: "Galdones lo aparta de la revisión y le da órdenes." },
  { id: "r-jacobo-prima", source: "jacobo", target: "prima-jacobo", type: "Familia", label: "Primos", certainty: "Hecho", strength: 1, details: "Jacobo la llama para completar la partida de naipes." },
];

const theories: TheoryRecord[] = [
  {
    id: "t-alessandra-cautiva",
    title: "La joven cautiva del capítulo 8 es Alessandra",
    status: "Muy probable",
    confidence: 97,
    thesis: "El montaje paralelo, el rescate de Anselmo y la confesión posterior de Alessandra forman una continuidad casi explícita, aunque el capítulo del pasado nunca pronuncia su nombre.",
    evidence: [
      { chapter: "8", text: "Anselmo encuentra a la niña de Ortía escondida y la entrega a su esposa encinta." },
      { chapter: "9", text: "Alessandra dice que Anselmo y su esposa la recibieron como hija." },
      { chapter: "11", text: "Su historia consciente comienza al quitarle las vendas y recuerda la noche en que casi murió." },
    ],
    counterpoint: "La equivalencia depende todavía de una elipsis; no aparece una identificación nominal dentro del capítulo 8.",
    characterIds: ["alessandra", "anselmo", "yolanda", "ortia", "astronomago"],
    tags: ["identidad", "pasado", "montaje"],
  },
  {
    id: "t-gemelos-verificacion",
    title: "Los gemelos no intentaban matar a Diego: lo estaban verificando",
    status: "Muy probable",
    confidence: 92,
    thesis: "El combate funciona como prueba de un estado llamado «despertar». Al obtener un resultado negativo, los gemelos se retiran aunque aún podrían continuar el ataque.",
    evidence: [
      { chapter: "10", text: "Uno declara «No despertó» y el otro responde que debieron mandar «una completa, no una media»." },
      { chapter: "11", text: "Habían seguido cada parada de Diego desde el inicio de la investigación." },
    ],
    counterpoint: "La prueba pudo coexistir con una orden de asesinato condicionada; el objetivo final no está formulado.",
    characterIds: ["diego", "gemelos", "jose", "lai"],
    tags: ["gemelos", "despertar", "prueba"],
  },
  {
    id: "t-hoz-sistema",
    title: "La hoz dorada conecta el culto antiguo con los gemelos",
    status: "Muy probable",
    confidence: 88,
    thesis: "El mismo símbolo/objeto aparece en el rito de hace veintiún años y cae de los gemelos tras el ataque, por lo que no es una simple joya diplomática.",
    evidence: [
      { chapter: "8", text: "Doce lechos de piedra tienen una hoz dorada; el Astronomago las usa para extraer órganos." },
      { chapter: "10", text: "Jacobo recoge una pequeña insignia de oro con forma de hoz donde combatieron los gemelos." },
      { chapter: "11", text: "Asmodelus juega con otra pieza dorada recibida de los gemelos." },
    ],
    counterpoint: "La segunda pieza se llama piocha, no hoz; podría ser moneda, herramienta o símbolo de otra jerarquía.",
    characterIds: ["gemelos", "astronomago", "jacobo", "asmodelus", "ortia"],
    tags: ["hoz dorada", "culto", "símbolo"],
  },
  {
    id: "t-makira-activacion",
    title: "Makira fue activada para provocar una respuesta en Diego",
    status: "Muy probable",
    confidence: 90,
    thesis: "El golpe en el hombro antecede una compulsión amorosa inmediata; el beso intenta completar una función que fracasa al encontrar algo inesperado en Diego.",
    evidence: [
      { chapter: "2", text: "El beso se siente como un cristal que se hace añicos y Diego sale de la atracción." },
      { chapter: "11", text: "Makira vincula el deseo al golpe y repite «Fallo, error» después de la ruptura." },
    ],
    counterpoint: "No se identifica a quien la golpeó ni se demuestra que los gemelos estuvieran detrás del contacto.",
    characterIds: ["makira", "diego", "gemelos", "alessandra"],
    tags: ["activación", "compulsión", "fallo"],
  },
  {
    id: "t-crimenes-misma-gramatica",
    title: "Los dos crímenes usan la misma gramática, pero no el mismo ejecutor",
    status: "Abierta",
    confidence: 78,
    thesis: "Ambas víctimas pierden los ojos mediante un agente de acción rápida, pero el buwano conserva marcas de guerra y quemaduras; Felisa muestra un procedimiento limpio y contenido.",
    evidence: [
      { chapter: "5", text: "El buwano presenta cuencas quemadas, fósforo tratado y extracción a través de los párpados." },
      { chapter: "9", text: "Felisa no fue asesinada donde apareció y sus cuencas fueron tratadas con herramientas más precisas." },
    ],
    counterpoint: "Las diferencias pueden deberse a una evolución del mismo asesino, a disponibilidad de herramientas o a una puesta en escena distinta.",
    characterIds: ["buwano", "felisa", "diego", "alvargio"],
    tags: ["víctimas", "ojos", "método"],
  },
  {
    id: "t-uruguita-red",
    title: "La uruguita es el puente material entre la guerra y el caso",
    status: "Abierta",
    confidence: 71,
    thesis: "El olor que Diego reconoce en las víctimas remite a los cañones malterios; al mismo tiempo, un diplomático intenta mover una carga de uruguita por el mismo puerto.",
    evidence: [
      { chapter: "Prólogo", text: "Los cañones queman pólvora, fósforo blanco y la llamada piedra del odio." },
      { chapter: "9", text: "Diego distingue en el buwano la guerra y en Felisa una versión lavada del mismo olor." },
      { chapter: "11", text: "Koyamï revela el encargo secreto de transportar uruguita." },
    ],
    counterpoint: "Todavía no existe una prueba física que identifique el residuo de los cuerpos como uruguita.",
    characterIds: ["diego", "koyami", "buwano", "felisa"],
    tags: ["uruguita", "olor", "guerra"],
  },
  {
    id: "t-artilleros-identidad",
    title: "Murmuradora y Chasqueadora siguen en Puerto Ámbar",
    status: "Abierta",
    confidence: 64,
    thesis: "Las criaturas perseguidas por Diego comparten la morfología exacta de los dos artilleros del capítulo 8 y una silueta alta parece vigilarlas o conducirlas.",
    evidence: [
      { chapter: "2", text: "Diego describe piel pálida, ojeras profundas, labios negros e incisivos rotos." },
      { chapter: "8", text: "Murmuradora y Chasqueadora presentan esos mismos rasgos antes de viajar a Ámbar." },
    ],
    counterpoint: "El grupo presente contiene varios artilleros y el texto no individualiza a los dos compañeros de la niña.",
    characterIds: ["murmuradora", "chasqueadora", "alessandra", "diego"],
    tags: ["artilleros", "identidad", "regreso"],
  },
];

const timeline: TimelineEvent[] = [
  { id: "ev-guerra", chapter: "Prólogo", when: "14 años antes", title: "Diego y Alvargio se conocen en el frente", summary: "Los cañones malterios, la piedra del odio y los artilleros fijan el trauma común.", characterIds: ["diego", "alvargio"], intensity: 5 },
  { id: "ev-cautiverio", chapter: "8", when: "21 años antes", title: "La niña vendada es trasladada a Ámbar", summary: "Murmuradora y Chasqueadora quedan separados de ella antes del rito.", characterIds: ["alessandra", "murmuradora", "chasqueadora", "ortia"], intensity: 5 },
  { id: "ev-redada", chapter: "8", when: "21 años antes", title: "Redada en la casona malteria", summary: "Diego resiste, Anselmo irrumpe y tres jóvenes sobreviven al Astronomago.", characterIds: ["diego", "anselmo", "alessandra", "astronomago", "lorenzo"], intensity: 5 },
  { id: "ev-presentacion", chapter: "1", when: "Primer día del carnaval", title: "El trío se reúne en el Hacha y Carcaj", summary: "Diego, José y Lài entran al carnaval mientras los gemelos ya observan.", characterIds: ["diego", "jose", "lai", "gemelos", "jacobo"], intensity: 2 },
  { id: "ev-makira", chapter: "2", when: "Primera noche", title: "El contacto con Makira falla", summary: "Un beso rompe la compulsión y deja una anomalía sin explicación.", characterIds: ["diego", "makira"], intensity: 3 },
  { id: "ev-buwano", chapter: "2", when: "Primera noche", title: "Aparece el buwano muerto", summary: "Diego persigue artilleros y tropieza con el cadáver erguido en la costanera.", characterIds: ["diego", "buwano"], intensity: 5 },
  { id: "ev-orden", chapter: "4", when: "Madrugada siguiente", title: "La Orden abre la investigación", summary: "Lorenzo advierte que la reaparición de artilleros puede romper la paz.", characterIds: ["diego", "alvargio", "lorenzo"], intensity: 3 },
  { id: "ev-autopsia", chapter: "5", when: "Mañana", title: "Examen del primer cadáver", summary: "Ojos extraídos, cuencas quemadas y llave de la hospedería de Clota.", characterIds: ["diego", "buwano", "galdones", "asmodelus"], intensity: 4 },
  { id: "ev-clota", chapter: "6", when: "Mañana", title: "Clota entrega el rastro del huésped", summary: "La habitación conserva pasaje, tarjeta de embarque y dinero de Buwe.", characterIds: ["diego", "clota", "buwano"], intensity: 2 },
  { id: "ev-barco", chapter: "7", when: "Tarde", title: "Diego interroga al Sueño de Pawné", summary: "El buwano no tuvo riñas; el barco confirma su ruta y su aislamiento.", characterIds: ["diego", "wolfrik", "taziri", "koyami", "buwano"], intensity: 3 },
  { id: "ev-felisa", chapter: "7", when: "Tarde", title: "Amis anuncia el hallazgo de Felisa", summary: "La muerte transforma la investigación en asunto familiar.", characterIds: ["amis", "alessandra", "diego", "felisa"], intensity: 5 },
  { id: "ev-examen-felisa", chapter: "9", when: "Tarde y noche", title: "El segundo método es más limpio", summary: "Diego concluye que Felisa fue trasladada y estuvo con alguien antes de morir.", characterIds: ["diego", "alessandra", "alvargio", "felisa", "guardia-felisa"], intensity: 5 },
  { id: "ev-ataque", chapter: "10", when: "Noche", title: "Los gemelos prueban a Diego", summary: "El ataque acaba con la frase «No despertó» y una hoz dorada en manos de Jacobo.", characterIds: ["diego", "gemelos", "jose", "lai", "jacobo"], intensity: 5 },
  { id: "ev-piocha", chapter: "11", when: "Mañana", title: "Asmodelus confiesa el rastro de los gemelos", summary: "Alessandra descubre que estuvieron en el pósito antes que Diego.", characterIds: ["alessandra", "asmodelus", "gemelos"], intensity: 4 },
  { id: "ev-uruguita", chapter: "11", when: "Mediodía", title: "Koyamï revela la carga cancelada", summary: "Un diplomático intentó mover cajas de uruguita por Puerto Ámbar.", characterIds: ["alessandra", "koyami"], intensity: 4 },
  { id: "ev-equipo", chapter: "11", when: "Mediodía", title: "Nace el equipo de tres", summary: "Alvargio une formalmente a Diego y Alessandra ante la amenaza de Utterdom.", characterIds: ["diego", "alessandra", "alvargio"], intensity: 3 },
  { id: "ev-lai-felisa", chapter: "12", when: "Después del carnaval, antes del amanecer", title: "El duelo secreto de Lài revela su relación con Felisa", summary: "Lài recuerda las visitas de Felisa, conserva su guante de encaje y se obliga a continuar otro día pese a sus pensamientos autodestructivos.", characterIds: ["lai", "felisa", "jose"], intensity: 5 },
];

const world: WorldRecord[] = [
  {
    id: "world-puerto-ambar",
    kind: "Ciudad/Lugar",
    name: "Puerto Ámbar",
    aliases: ["Ámbar"],
    summary: "Gran puerto de Thet y escenario principal de la investigación durante el carnaval.",
    geography: "Ciudad costera extensa, recorrida por una costanera y conectada con rutas marítimas internacionales.",
    government: "La guardia del puerto está comandada por Anselmo Serdán. La Cofradía del Sacro Hierro mantiene presencia religiosa e investigativa.",
    peoples: "Población portuaria y visitantes de numerosos reinos reunidos por el carnaval.",
    culture: "Carnaval multitudinario, tabernas, comercio callejero, hospederías y una vida nocturna intensa.",
    economy: "Puerto comercial, hospedaje, mercados, espectáculos y tráfico de mercancías entre reinos.",
    currency: "Quadrac y quac aparecen como monedas; su equivalencia todavía no está establecida.",
    languages: "No establecido.",
    religions: "Cofradía del Sacro Hierro y Capilla del Sol; también circulan cultos extranjeros.",
    military: "Guardia del puerto; la Orden interviene cuando el caso amenaza la paz entre reinos.",
    history: "Hace veintiún años, una redada interrumpió un rito malterio en una casona de la ciudad.",
    relations: "Recibe barcos de Brasan, Iskanat y otros puertos; la investigación roza intereses de Buwe, Utterdom, Malteria y Shuzún.",
    locations: "Hacha y Carcaj; Casa Bajamar; Capilla del Sol; alcázar; costanera; pósito; hospedería de Clota; Sueño de Pawné.",
    conflicts: "Asesinatos rituales, reaparición de artilleros, tráfico de uruguita y riesgo diplomático.",
    notes: "Ficha de síntesis construida con los capítulos disponibles; los campos no establecidos quedan abiertos a edición.",
    tags: ["Thet", "puerto", "carnaval", "investigación"],
  },
  {
    id: "world-felsenthrone",
    kind: "País/Reino",
    name: "Felsenthrone",
    aliases: ["Felsenthron", "Monarquía glacial del noroeste"],
    summary: "Monarquía reglamentaria cuya identidad política se resume en el edicto «Incambiable como las montañas».",
    geography: "Montañas del Trono Negro, fortaleza de Eisenkrone, Glaciares Cantores, Valle Albar y Río Blanco. Inviernos de −30 °C a −45 °C y verano oficial de +12 °C.",
    government: "Rey de Piedra, siempre primogénito varón de la línea Hjaldursblod, y Asamblea de los Doce Hielos elegida cada veinte años.",
    peoples: "Nobleza, burguesía y campesinado distinguidos por indumentaria y marcas reglamentarias.",
    culture: "La obediencia se expresa en arquitectura, vestimenta, medidas y producción estrictamente codificadas.",
    economy: "Molinería de harina grado A y textiles de lino ártico. Intercambia acero y productos de clima frío con Aljanubar.",
    currency: "No establecida en el dossier.",
    languages: "No establecidas; el embajador Idris al-Felsani habla alemán antiguo.",
    religions: "No establecidas en el dossier.",
    military: "No detallado en el dossier.",
    history: "Brugheim figura como fundado en el Año del Gran Frío. El documento está sellado por Magni VII Hjaldursblod en el año 543 del Reinado de Piedra.",
    relations: "Tratado comercial pragmático con Aljanubar: recibe sal rosa y entrega acero para irrigación.",
    locations: "Eisenkrone; Montañas del Trono Negro; Glaciares Cantores; Valle Albar; Río Blanco; Brugheim; Molino Vinterhald; Factoría Dansiah.",
    conflicts: "El exilio a las Llanuras de Hielo Eterno castiga infracciones a las normas inmutables.",
    notes: "La Aurora de Piedra se atribuye a torio en los glaciares; los Glaciares Cantores vibran a 18 Hz durante lunas llenas.",
    tags: ["hielo", "monarquía", "reglamentación", "Brugheim"],
  },
  {
    id: "world-aljanubar",
    kind: "País/Reino",
    name: "Aljanubar",
    aliases: ["Reino teocrático de Aljanubar", "Al-Mizan"],
    summary: "Teocracia lunar organizada en torno al equilibrio cósmico y al lema «Vivimos por la Balanza».",
    geography: "Desiertos, oasis, costa y el puerto estratégico de Bahr Nur, dividido entre la Cofradía y fanáticos.",
    government: "Al-Murshid al-A'zam, elegido tras cuarenta días de ayuno, gobierna con el Majlis Al-Hikma de doce Huffaz.",
    peoples: "Al-Ruhban o sacerdotes, Al-Tujjar o mercaderes y Al-Fallahin o campesinos.",
    culture: "Laylat Al-Mizan, 'Id Al-Zillal, astronomía sagrada, reciprocidad jurídica y símbolos de espiral y medialuna.",
    economy: "Comercio de madera de terramar, higos, dátiles, antimonio, miel, sal negra y sal rosa; existe esclavitud por deudas.",
    currency: "Quadrac.",
    languages: "Árabe ritual y nombres locales; no se especifica una lengua oficial separada.",
    religions: "Culto de Al-Mizan y Templo de las Lunas Gemelas; tensión con misioneros de Shuzu.",
    military: "Jund Al-Mizan: Guardianes de la Arena en meharis y Celadores del Mar en dhows ceremoniales.",
    history: "Mantiene un tratado comercial con Felsenthrone y una estructura teocrática sostenida por instituciones sagradas.",
    relations: "Relación pragmática con Felsenthrone y tensión religiosa contenida con Shuzu por interés comercial.",
    locations: "Bahr Nur; Mercado Flotante de Al-Nur; Barrio de las Caravanas; Templo de las Lunas Gemelas; Muertos del Silencio; Coral Negro; Plaza de los Susurrantes; minas de sal; Puerto Fantasma; Barrio del Azafrán; Abismo de los Naufragios.",
    conflicts: "Fanáticos, Cofradía, contrabando, patrocinadores externos, esclavitud por deudas y rutas clandestinas.",
    notes: "La Balanza de la Verdad se guarda bajo el Gran Templo. Idris al-Felsani es embajador en Felsenthrone.",
    tags: ["teocracia", "desierto", "Bahr Nur", "comercio", "quadrac"],
  },
  {
    id: "world-shuzun",
    kind: "País/Reino",
    name: "Imperio de Shuzún",
    aliases: ["Shuzun", "Shuzu"],
    summary: "Imperio fértil del norte de Fenghai, centro espiritual y comercial de la Ruta del Jade.",
    geography: "Bosques de cedros azules de Lánshā, cordillera de Linglong, Mar Interior de Ofiudthal y río Liánhuā; clima templado, inviernos suaves y veranos lluviosos.",
    government: "Monarquía divina de Lóng Wéi Huangdi, decimocuarto emperador de la dinastía Xīngzhé, desde el Palacio de Jade Susurrante.",
    peoples: "Comunidades agrícolas, ciudades amuralladas, monjes Xiuqìzhě y Guardia del Trueno Escarlata.",
    culture: "Geomancia, caligrafía protectora, Festival del Primer Aliento y Juicio de las Tres Lunas.",
    economy: "Ruta del Jade; exporta Seda de Lunaria, Porcelana de Xīnyǎn e Hierro Espiritual. La navegación interior se limita a pesca costera en balsas de bambú.",
    currency: "No establecida en el dossier.",
    languages: "No se nombra una lengua oficial; el dossier conserva títulos y nombres propios shuzuneses.",
    religions: "Senda del Aliento Celestial, equilibrio entre Qi del Cielo y Qi de la Tierra, culto a Shénlóng y Liánhuī.",
    military: "Guardia del Trueno Escarlata, entrenada desde la infancia en la espada Jiànfēng.",
    history: "La dinastía Xīngzhé reclama descendencia del dragón Shénlóng y de la diosa Liánhuī.",
    relations: "Desconfía de Hornia, mantiene alianzas frágiles con clanes de Naghei y observa con curiosidad a Mil.",
    locations: "Palacio de Jade Susurrante; Lánshā; Linglong; Ofiudthal; Liánhuā; Huǒshān; Templo de las Nubes Rotas.",
    conflicts: "Disputas con Hornia por la Ruta del Jade y choque entre espiritualismo shuzunés y tecnología de Mil.",
    notes: "Xu Zua pertenece a la Orden del Viento Cálido. Leyendas centrales: Dragón Durmiente y Río de Lágrimas.",
    tags: ["imperio", "Fenghai", "Ruta del Jade", "Lài", "Xiuqìzhě"],
  },
  {
    id: "world-genesis",
    kind: "Cosmología",
    name: "Proyecto Génesis · Nave Arca",
    aliases: ["El Núcleo", "Hábitat simulado", "Última iteración"],
    summary: "Secreto de fondo: el mundo es un hábitat artificial del tamaño de Australia dentro de una nave-arca, creado como último intento de supervivencia humana.",
    geography: "Isla artificial con varios reinos, montañas-frontera y zonas inexplorables. Un domo controla luz, clima y estaciones.",
    government: "La IA supervisora llamada El Silencio controla clima, mantenimiento, evaluación genética y registros culturales.",
    peoples: "Humanos nacidos dentro del sistema sin conocimiento del origen artificial de su mundo.",
    culture: "Lenguas, culturas e historias fueron fabricadas y sembradas por la IA con variaciones deliberadas.",
    economy: "No aplica a escala de la nave; cada reino desarrolla su propia economía dentro del hábitat.",
    currency: "Variable por reino; no definida a escala del Proyecto Génesis.",
    languages: "Fabricadas y sembradas por la IA durante la fase inicial de cada ciclo.",
    religions: "Sabios, sacerdotes y oráculos pueden ser terminales encubiertos o androides que dispersan información controlada.",
    military: "No establecido.",
    history: "Existen ciclos anteriores desconocidos, reinicios parciales y residuos de iteraciones fallidas.",
    relations: "El objetivo final es que protagonistas de distintas historias descubran la verdad, despierten el arca y completen el viaje a un planeta real.",
    locations: "Banco genético; úteros artificiales; zonas de ruptura; áreas ocultas de mantenimiento; ruinas tecnológicas disfrazadas de magia.",
    conflicts: "Apocalipsis programado, fallas estructurales, plagas, cataclismos y división entre quienes acepten, nieguen o intenten controlar la verdad.",
    notes: "El dossier afirma que esta iteración es la última. La magia es emergente y psíquica, no ingeniería ni tecnología.",
    tags: ["secreto", "nave arca", "IA", "ciclos", "spoiler"],
  },
];

const magicSystems: MagicSystemRecord[] = [
  {
    id: "magic-hakvar",
    name: "Hakvar / Hakvia",
    category: "Hados · esperanza y protección",
    status: "Canónico",
    source: "Dossier Urug/Hakvar · Proyecto Génesis",
    principle: "Manipulación inconsciente del destino mediante pactos o susurros de los hiladores; representa esperanza y protección.",
    access: "No formalizado. El roadmap prevé que Diego lo invoque de manera genuina.",
    cost: "Fatiga cardíaca.",
    limits: "La protección requiere contención del aliento al interactuar con Sulvar; el abuso colapsa el sistema.",
    manifestations: "Protección y alteración de los hados.",
    materials: "No establecidos.",
    institutions: "No establecidas.",
    users: "Diego, en desarrollo narrativo previsto.",
    risks: "Daño cardíaco y colapso por uso excesivo.",
    history: "Forma parte de los sistemas emergentes del hábitat y del conflicto final con el Urug.",
    notes: "Órgano asociado: corazón. En el ritual del Urug, extraerlo rompe la esperanza y el vínculo emocional con la vida.",
    evidence: [{ chapter: "Dossier Urug/Hakvar", text: "Hakvar/Hakvia representa esperanza y protección; su órgano asociado es el corazón." }],
    tags: ["corazón", "protección", "esperanza", "hados"],
  },
  {
    id: "magic-sulvar",
    name: "Sulvar / Sulvia",
    category: "Miedo y ocultamiento",
    status: "Canónico",
    source: "Dossier Urug/Hakvar",
    principle: "Representa el miedo y el ocultamiento; el silencio permite ver más allá de lo evidente.",
    access: "No formalizado. Alessandra lo usa brevemente según el roadmap del dossier.",
    cost: "Asfixia ficticia y tos seca.",
    limits: "Su interacción con Hakvar exige contención del aliento; el abuso puede colapsar el sistema.",
    manifestations: "Ocultamiento, silencio y percepción intuitiva.",
    materials: "No establecidos.",
    institutions: "No establecidas.",
    users: "Alessandra, según el dossier narrativo.",
    risks: "Sensación de asfixia y tos seca.",
    history: "El dossier asocia a Alessandra con Sulvia como contrapunto intuitivo al racionalismo de Diego.",
    notes: "Órgano asociado: pulmones. En el ritual, su extracción apaga la voz interna y externa y silencia el alma.",
    evidence: [{ chapter: "Dossier Urug/Hakvar", text: "Sulvar/Sulvia representa miedo y ocultamiento; su órgano asociado son los pulmones." }],
    tags: ["pulmones", "miedo", "ocultamiento", "silencio"],
  },
  {
    id: "magic-votar",
    name: "Vötar / Votia",
    category: "Negociación y deuda",
    status: "Canónico",
    source: "Dossier Urug/Hakvar",
    principle: "Representa la negociación, la deuda y los pactos.",
    access: "No establecido.",
    cost: "Dolor abdominal e ictericia.",
    limits: "Urugar puede romper los pactos de Vötar y liberar toxinas emocionales y físicas.",
    manifestations: "Contratos, negociación y deuda.",
    materials: "No establecidos.",
    institutions: "No establecidas.",
    users: "No establecidos.",
    risks: "Daño hepático, dolor abdominal e ictericia.",
    history: "No desarrollada todavía en los documentos.",
    notes: "Órgano asociado: hígado. En el ritual, extraerlo rompe los pactos y aísla el alma.",
    evidence: [{ chapter: "Dossier Urug/Hakvar", text: "Vötar/Votia representa negociación y deuda; su órgano asociado es el hígado." }],
    tags: ["hígado", "deuda", "pactos", "negociación"],
  },
  {
    id: "magic-urugar",
    name: "Urugar / Urugia",
    category: "Odio y destrucción",
    status: "Canónico",
    source: "Dossier Urug/Hakvar",
    principle: "Representa el odio y la destrucción. El Urug es la manifestación de la rabia colectiva humana.",
    access: "Puede liberarse mediante una extracción ritual secuencial de órganos.",
    cost: "Úlceras y cólicos; la liberación completa destruye al portador.",
    limits: "El orden ritual es obligatorio: ojos, corazón, pulmones, hígado, vesícula biliar y cerebro.",
    manifestations: "Rabia colectiva, corrupción y destrucción.",
    materials: "Uruguita o piedra del odio aparece vinculada al caso, aunque la equivalencia exacta con Urugar sigue abierta.",
    institutions: "Congregación del Astronomago y actores aún no identificados.",
    users: "No establecidos; el roadmap prevé una activación incontrolable.",
    risks: "Corrompe cuanto tiene cerca y culmina en destrucción inevitable al retirar el cerebro.",
    history: "El rito del pasado en Puerto Ámbar y los asesinatos actuales comparten una gramática de extracción.",
    notes: "Órgano asociado: vesícula biliar. Su extracción libera la semilla de rabia en un cuerpo ya corrompido.",
    evidence: [{ chapter: "Dossier Urug/Hakvar", text: "Urugar/Urugia representa odio y destrucción; su órgano asociado es la vesícula biliar." }],
    tags: ["vesícula", "odio", "Urug", "uruguita", "ritual"],
  },
  {
    id: "magic-zirru",
    name: "Zirru",
    category: "Fuente de toda magia",
    status: "Secreto",
    source: "Dossier Urug/Hakvar",
    principle: "Fuente de toda magia.",
    access: "No establecido.",
    cost: "Liberación incontrolable.",
    limits: "La extracción del cerebro disuelve el último sello del ritual.",
    manifestations: "Liberación final de la fuerza contenida.",
    materials: "No establecidos.",
    institutions: "No establecidas.",
    users: "No establecidos.",
    risks: "El Urug se libera a través del cuerpo del portador y lo destruye.",
    history: "No desarrollada todavía en los documentos.",
    notes: "Órgano asociado: cerebro.",
    evidence: [{ chapter: "Dossier Urug/Hakvar", text: "Zirru figura como fuente de toda magia y se asocia al cerebro." }],
    tags: ["cerebro", "fuente", "liberación", "secreto"],
  },
  {
    id: "magic-silencio",
    name: "Magia del Silencio",
    category: "Pactos de sangre y control",
    status: "Secreto",
    source: "Dossier Proyecto Génesis",
    principle: "Sistema atado a pactos de sangre y controlado por una casta leal al poder.",
    access: "Mediante pactos de sangre; los requisitos precisos no están establecidos.",
    cost: "No establecido.",
    limits: "No establecidas.",
    manifestations: "No establecidas.",
    materials: "Sangre, según la naturaleza de sus pactos.",
    institutions: "Casta leal al poder, todavía sin nombre.",
    users: "No establecidos.",
    risks: "Sometimiento a una estructura de poder; otros riesgos no establecidos.",
    history: "Surge como consecuencia evolutiva y psíquica del encierro humano en la nave.",
    notes: "El dossier aclara que la magia no es tecnología, aunque resuena con patrones estructurales del hábitat.",
    evidence: [{ chapter: "Proyecto Génesis", text: "La Magia del Silencio está atada a pactos de sangre y controlada por una casta leal al poder." }],
    tags: ["sangre", "pacto", "casta", "secreto"],
  },
  {
    id: "magic-vinculaciones",
    name: "Vinculaciones Elegidas",
    category: "Contratos y energía vital",
    status: "Secreto",
    source: "Dossier Proyecto Génesis",
    principle: "Sistema reglamentado basado en contratos, costos y energía vital.",
    access: "Mediante vinculaciones elegidas y contratos; el procedimiento no está establecido.",
    cost: "Energía vital.",
    limits: "Reglas contractuales aún no detalladas.",
    manifestations: "No establecidas.",
    materials: "No establecidos.",
    institutions: "No establecidas.",
    users: "No establecidos.",
    risks: "Agotamiento o pérdida de energía vital; alcance exacto no establecido.",
    history: "Surge como consecuencia evolutiva y psíquica del encierro humano en la nave.",
    notes: "La magia parece resonar con patrones estructurales del hábitat.",
    evidence: [{ chapter: "Proyecto Génesis", text: "Las Vinculaciones Elegidas se describen como magia reglamentada por contratos, costos y energía vital." }],
    tags: ["contratos", "energía vital", "reglas", "secreto"],
  },
];

export const initialArchive: ArchiveState = {
  dataVersion: 13,
  title: "Proyecto Neo",
  profile: {
    archiveTitle: "Proyecto Neo",
    storyTitle: "Las desventuras del hidalgo don Diego de Montemar",
    subtitle: "Biblia narrativa y construcción del mundo",
    projectLabel: "Proyecto narrativo · Construcción del mundo",
    homeHeading: "Red de personajes y mundo",
    location: "Puerto Ámbar",
    author: "",
    genre: "Fantasía sucia · misterio",
    status: "En desarrollo",
    synopsis: "Un veterano convertido en investigador sigue una cadena de asesinatos rituales en Puerto Ámbar mientras viejas armas, pactos y secretos de su mundo vuelven a despertar.",
    chapterLabels: [...chapterLabels],
    theme: "grim",
    activeThemeId: "grim",
    customThemes: [],
    spotifyPlaylistUrl: "",
    youtubeAmbientUrl: "",
    coverImageDataUrl: "",
    bannerImageDataUrl: "",
    boardIconDataUrl: "",
    boardViewport: { scale: 0.2, x: 44, y: 28, worldSpace: true },
    sceneBoardViewport: { scale: 1, x: 18, y: 18, worldSpace: true },
    sceneBoardCompact: false,
    manuscriptLayout: { ...defaultManuscriptLayout },
    writingAnalysis: { ...defaultWritingAnalysis, fillerPhrases: [...defaultWritingAnalysis.fillerPhrases] },
  },
  manuscript: {
    fileName: "Neo las desventuras (lo que llevo)(10).docx + Capítulo 12.docx",
    words: 35352,
    chapters: 12,
    updatedLabel: "4 de agosto de 2026",
  },
  characters,
  relationships,
  theories,
  timeline,
  world,
  magicSystems,
  worldTexts: [],
  magicTexts: [],
  writingChapters: [],
  maps: [],
  heatmap: {
    title: "Temperatura narrativa",
    description: "Variables editoriales a lo largo del manuscrito. Puedes cambiar filas, columnas y valores.",
    columnLabels: [...chapterLabels],
  },
  narrativeHeat: [
    { label: "Violencia", values: [5, 1, 4, 2, 1, 3, 2, 2, 5, 5, 5, 2, 1], note: "Combate, amenaza física y daño corporal." },
    { label: "Misterio", values: [3, 2, 5, 4, 5, 5, 3, 4, 5, 5, 5, 5, 3], note: "Cantidad y densidad de preguntas abiertas." },
    { label: "Carga emocional", values: [3, 3, 3, 4, 3, 2, 4, 4, 5, 5, 4, 5, 5], note: "Duelo, intimidad, trauma y vínculos." },
    { label: "Información nueva", values: [4, 4, 5, 5, 5, 5, 4, 5, 5, 5, 5, 5, 4], note: "Pistas, antecedentes o cambios de lectura." },
    { label: "Anomalía", values: [4, 2, 5, 5, 3, 3, 1, 4, 5, 4, 5, 5, 2], note: "Elementos que desbordan la explicación policial inmediata." },
  ],
};

type LegacyWritingChapter = {
  id: string;
  label?: string;
  title?: string;
  content?: string;
  status?: "Borrador" | "Revisión" | "Final";
  updatedAt?: string;
  order?: number;
  scenes?: WritingScene[];
};

function normalizeWritingChapters(chapters: LegacyWritingChapter[] | undefined): WritingChapter[] {
  if (!Array.isArray(chapters)) return [];
  return chapters
    .map((chapter, chapterIndex) => {
      const id = typeof chapter.id === "string" && chapter.id ? chapter.id : `capitulo-migrado-${chapterIndex + 1}`;
      const sourceScenes = Array.isArray(chapter.scenes)
        ? chapter.scenes
        : [{
            id: `${id}-escena-1`,
            chapterId: id,
            order: 0,
            title: "Escena 1",
            content: chapter.content ?? "",
            pov: "",
            location: "",
            narrativeLayer: "",
            status: chapter.status ?? "Borrador",
            updatedAt: chapter.updatedAt ?? new Date(0).toISOString(),
          } satisfies WritingScene];
      const scenes = sourceScenes
        .map((scene, sceneIndex) => ({
          id: scene.id || `${id}-escena-${sceneIndex + 1}`,
          chapterId: id,
          order: Number.isFinite(scene.order) ? scene.order : sceneIndex,
          title: scene.title || `Escena ${sceneIndex + 1}`,
          content: scene.content ?? "",
          pov: scene.pov ?? "",
          location: scene.location ?? "",
          narrativeLayer: scene.narrativeLayer ?? "",
          status: scene.status ?? "Borrador",
          updatedAt: scene.updatedAt ?? new Date(0).toISOString(),
        }))
        .sort((a, b) => a.order - b.order)
        .map((scene, order) => ({ ...scene, order }));
      return {
        id,
        label: chapter.label || `Capítulo ${chapterIndex + 1}`,
        title: chapter.title || "Sin título",
        order: Number.isFinite(chapter.order) ? chapter.order! : chapterIndex,
        scenes,
      } satisfies WritingChapter;
    })
    .sort((a, b) => a.order - b.order)
    .map((chapter, order) => ({ ...chapter, order }));
}

function normalizedWritingAnalysis(profile?: ProjectProfile): WritingAnalysisSettings {
  const repetitionWindow = profile?.writingAnalysis?.repetitionWindow;
  const fillerPhrases = profile?.writingAnalysis?.fillerPhrases;
  return {
    repetitionWindow: Number.isFinite(repetitionWindow) ? Math.max(10, Math.min(100, Math.round(repetitionWindow!))) : defaultWritingAnalysis.repetitionWindow,
    fillerPhrases: Array.isArray(fillerPhrases) && fillerPhrases.length > 0
      ? fillerPhrases.map((item) => item.trim()).filter(Boolean).slice(0, 80)
      : [...defaultWritingAnalysis.fillerPhrases],
  };
}

function normalizedManuscriptLayout(profile?: ProjectProfile): ManuscriptLayout {
  const source = profile?.manuscriptLayout;
  const number = (value: number | undefined, fallback: number, min: number, max: number) => Number.isFinite(value) ? Math.max(min, Math.min(max, value!)) : fallback;
  return {
    ...defaultManuscriptLayout,
    ...(source ?? {}),
    pageWidthMm: number(source?.pageWidthMm, defaultManuscriptLayout.pageWidthMm, 90, 320),
    pageHeightMm: number(source?.pageHeightMm, defaultManuscriptLayout.pageHeightMm, 120, 450),
    marginTopMm: number(source?.marginTopMm, defaultManuscriptLayout.marginTopMm, 5, 60),
    marginRightMm: number(source?.marginRightMm, defaultManuscriptLayout.marginRightMm, 5, 60),
    marginBottomMm: number(source?.marginBottomMm, defaultManuscriptLayout.marginBottomMm, 5, 60),
    marginLeftMm: number(source?.marginLeftMm, defaultManuscriptLayout.marginLeftMm, 5, 60),
    fontSizePt: number(source?.fontSizePt, defaultManuscriptLayout.fontSizePt, 8, 22),
    lineHeight: number(source?.lineHeight, defaultManuscriptLayout.lineHeight, 1, 2.2),
    paragraphIndentMm: number(source?.paragraphIndentMm, defaultManuscriptLayout.paragraphIndentMm, 0, 18),
  };
}

function worldBoardPosition(position: CharacterRecord["board"]) {
  const percentageCoordinates = position.x >= -5 && position.x <= 105 && position.y >= -5 && position.y <= 105;
  if (!percentageCoordinates) return position;
  const marginX = 260;
  const marginY = 210;
  return {
    ...position,
    x: Math.round(marginX + Math.max(0, Math.min(100, position.x)) / 100 * (BOARD_WORLD_WIDTH - marginX * 2)),
    y: Math.round(marginY + Math.max(0, Math.min(100, position.y)) / 100 * (BOARD_WORLD_HEIGHT - marginY * 2)),
  };
}

function normalizedBoardViewport(viewport?: BoardViewport): BoardViewport {
  if (viewport?.worldSpace) return {
    scale: Number.isFinite(viewport.scale) ? Math.max(0.12, Math.min(3.5, viewport.scale)) : 0.2,
    x: Number.isFinite(viewport.x) ? viewport.x : 44,
    y: Number.isFinite(viewport.y) ? viewport.y : 28,
    worldSpace: true,
  };
  return { scale: 0.2, x: 44, y: 28, worldSpace: true };
}

function normalizedSceneBoardViewport(viewport?: BoardViewport): BoardViewport {
  return {
    scale: Number.isFinite(viewport?.scale) ? Math.max(0.35, Math.min(1.8, viewport!.scale)) : 1,
    x: Number.isFinite(viewport?.x) ? viewport!.x : 18,
    y: Number.isFinite(viewport?.y) ? viewport!.y : 18,
    worldSpace: true,
  };
}

export function migrateArchive(state: ArchiveState): ArchiveState {
  const version = state.dataVersion ?? 1;
  const candidate = state as ArchiveState & {
    profile?: ProjectProfile;
    world?: WorldRecord[];
    magicSystems?: MagicSystemRecord[];
    worldTexts?: ImportedTextRecord[];
    magicTexts?: ImportedTextRecord[];
    writingChapters?: LegacyWritingChapter[];
    maps?: WorldMapRecord[];
    heatmap?: HeatmapConfig;
  };
  if (version >= 13 && candidate.profile && Array.isArray(candidate.world) && Array.isArray(candidate.magicSystems) && Array.isArray(candidate.worldTexts) && Array.isArray(candidate.magicTexts) && Array.isArray(candidate.writingChapters)) {
    return state;
  }
  if (version >= 10 && candidate.profile && Array.isArray(candidate.world) && Array.isArray(candidate.magicSystems) && Array.isArray(candidate.worldTexts) && Array.isArray(candidate.magicTexts) && Array.isArray(candidate.writingChapters)) {
    return {
      ...state,
      dataVersion: 13,
      profile: {
        ...candidate.profile,
        activeThemeId: candidate.profile.activeThemeId ?? candidate.profile.theme,
        customThemes: candidate.profile.customThemes ?? [],
        coverImageDataUrl: candidate.profile.coverImageDataUrl ?? "",
        bannerImageDataUrl: candidate.profile.bannerImageDataUrl ?? "",
        boardIconDataUrl: candidate.profile.boardIconDataUrl ?? "",
        boardViewport: normalizedBoardViewport(candidate.profile.boardViewport),
        sceneBoardViewport: normalizedSceneBoardViewport(candidate.profile.sceneBoardViewport),
        sceneBoardCompact: candidate.profile.sceneBoardCompact ?? false,
        manuscriptLayout: normalizedManuscriptLayout(candidate.profile),
        writingAnalysis: normalizedWritingAnalysis(candidate.profile),
      },
      characters: state.characters.map((character) => ({ ...character, board: worldBoardPosition(character.board) })),
      writingChapters: normalizeWritingChapters(candidate.writingChapters),
      maps: Array.isArray(candidate.maps) ? candidate.maps : [],
    };
  }

  if (version >= 5 && candidate.profile && Array.isArray(candidate.world) && Array.isArray(candidate.magicSystems)) {
    const columnLabels = candidate.heatmap?.columnLabels?.length
      ? candidate.heatmap.columnLabels
      : candidate.profile.chapterLabels.length > 0 ? candidate.profile.chapterLabels : ["1"];
    return {
      ...state,
      dataVersion: 13,
      profile: {
        ...candidate.profile,
        spotifyPlaylistUrl: candidate.profile.spotifyPlaylistUrl ?? "",
        youtubeAmbientUrl: candidate.profile.youtubeAmbientUrl ?? "",
        activeThemeId: candidate.profile.activeThemeId ?? candidate.profile.theme,
        customThemes: candidate.profile.customThemes ?? [],
        coverImageDataUrl: candidate.profile.coverImageDataUrl ?? "",
        bannerImageDataUrl: candidate.profile.bannerImageDataUrl ?? "",
        boardIconDataUrl: candidate.profile.boardIconDataUrl ?? "",
        boardViewport: normalizedBoardViewport(candidate.profile.boardViewport),
        sceneBoardViewport: normalizedSceneBoardViewport(candidate.profile.sceneBoardViewport),
        sceneBoardCompact: candidate.profile.sceneBoardCompact ?? false,
        manuscriptLayout: normalizedManuscriptLayout(candidate.profile),
        writingAnalysis: normalizedWritingAnalysis(candidate.profile),
        subtitle: candidate.profile.subtitle === "Investigación narrativa y biblia del mundo"
          ? "Biblia narrativa y construcción del mundo"
          : candidate.profile.subtitle,
        projectLabel: candidate.profile.projectLabel === "Orden del Sacro Hierro · Cuaderno de pesquisas"
          ? "Proyecto narrativo · Construcción del mundo"
          : candidate.profile.projectLabel,
        homeHeading: candidate.profile.homeHeading === "La red detrás del caso"
          ? "Red de personajes y mundo"
          : candidate.profile.homeHeading,
      },
      characters: state.characters.map((character) => ({ ...character, board: worldBoardPosition(character.board) })),
      world: candidate.world.map((record) => ({ ...record, attachments: record.attachments ?? [] })),
      magicSystems: candidate.magicSystems.map((system) => ({ ...system, attachments: system.attachments ?? [] })),
      worldTexts: Array.isArray(candidate.worldTexts) ? candidate.worldTexts : [],
      magicTexts: Array.isArray(candidate.magicTexts) ? candidate.magicTexts : [],
      writingChapters: normalizeWritingChapters(candidate.writingChapters),
      maps: Array.isArray(candidate.maps) ? candidate.maps : [],
      heatmap: candidate.heatmap ?? {
        title: "Temperatura narrativa",
        description: "Variables editoriales a lo largo del manuscrito. Puedes cambiar filas, columnas y valores.",
        columnLabels,
      },
      narrativeHeat: state.narrativeHeat.map((row) => ({
        ...row,
        values: columnLabels.map((_, index) => row.values[index] ?? 0),
      })),
    };
  }

  const laiFelisa = relationships.find((relationship) => relationship.id === "r-lai-felisa")!;
  const laiFelisaEvent = timeline.find((event) => event.id === "ev-lai-felisa")!;
  const canonicalLai = characters.find((character) => character.id === "lai")!;
  const canonicalFelisa = characters.find((character) => character.id === "felisa")!;
  const relationExists = state.relationships.some((relationship) => relationship.id === laiFelisa.id);
  const eventExists = state.timeline.some((event) => event.id === laiFelisaEvent.id);

  const migratedCharacters = state.characters.map((character) => {
    const canonical = character.id === "lai" ? canonicalLai : character.id === "felisa" ? canonicalFelisa : null;
    const chapter12 = canonical?.evidence.find((item) => item.chapter === "12");
    const presence = Array.from({ length: chapterLabels.length }, (_, index) => {
      if (index === chapterLabels.length - 1 && canonical) return canonical.presence[index] ?? 0;
      return character.presence[index] ?? 0;
    });
    if (!canonical) return { ...character, presence };
    return {
      ...character,
      summary: canonical.summary,
      background: canonical.background,
      evidence: chapter12 && !character.evidence.some((item) => item.chapter === "12")
        ? [...character.evidence, { ...chapter12 }]
        : character.evidence,
      presence,
    };
  });

  return {
    ...state,
    dataVersion: 13,
    title: state.title || initialArchive.title,
    profile: {
      ...initialArchive.profile,
      ...(candidate.profile ?? {}),
      archiveTitle: candidate.profile?.archiveTitle || state.title || initialArchive.profile.archiveTitle,
      spotifyPlaylistUrl: candidate.profile?.spotifyPlaylistUrl ?? "",
      youtubeAmbientUrl: candidate.profile?.youtubeAmbientUrl ?? "",
      activeThemeId: candidate.profile?.activeThemeId ?? candidate.profile?.theme ?? initialArchive.profile.theme,
      customThemes: candidate.profile?.customThemes ?? [],
      coverImageDataUrl: candidate.profile?.coverImageDataUrl ?? "",
      bannerImageDataUrl: candidate.profile?.bannerImageDataUrl ?? "",
      boardIconDataUrl: candidate.profile?.boardIconDataUrl ?? "",
      boardViewport: normalizedBoardViewport(candidate.profile?.boardViewport),
      sceneBoardViewport: normalizedSceneBoardViewport(candidate.profile?.sceneBoardViewport),
      sceneBoardCompact: candidate.profile?.sceneBoardCompact ?? false,
      manuscriptLayout: normalizedManuscriptLayout(candidate.profile),
      writingAnalysis: normalizedWritingAnalysis(candidate.profile),
      subtitle: candidate.profile?.subtitle === "Investigación narrativa y biblia del mundo"
        ? "Biblia narrativa y construcción del mundo"
        : candidate.profile?.subtitle || initialArchive.profile.subtitle,
      projectLabel: candidate.profile?.projectLabel === "Orden del Sacro Hierro · Cuaderno de pesquisas"
        ? "Proyecto narrativo · Construcción del mundo"
        : candidate.profile?.projectLabel || initialArchive.profile.projectLabel,
      homeHeading: candidate.profile?.homeHeading === "La red detrás del caso"
        ? "Red de personajes y mundo"
        : candidate.profile?.homeHeading || initialArchive.profile.homeHeading,
    },
    manuscript: {
      ...initialArchive.manuscript,
      ...state.manuscript,
      fileName: state.manuscript.fileName === "Neo las desventuras (lo que llevo)(10).docx"
        ? initialArchive.manuscript.fileName
        : state.manuscript.fileName,
      words: Math.max(state.manuscript.words || 0, initialArchive.manuscript.words),
      chapters: Math.max(state.manuscript.chapters || 0, 12),
    },
    characters: migratedCharacters.map((character) => ({ ...character, board: worldBoardPosition(character.board) })),
    relationships: relationExists
      ? state.relationships.map((relationship) => relationship.id === laiFelisa.id ? { ...laiFelisa } : relationship)
      : [...state.relationships, { ...laiFelisa }],
    timeline: eventExists ? state.timeline : [...state.timeline, { ...laiFelisaEvent }],
    world: (Array.isArray(candidate.world) ? candidate.world : JSON.parse(JSON.stringify(world)) as WorldRecord[]).map((record) => ({ ...record, attachments: record.attachments ?? [] })),
    magicSystems: (Array.isArray(candidate.magicSystems) ? candidate.magicSystems : JSON.parse(JSON.stringify(magicSystems)) as MagicSystemRecord[]).map((system) => ({ ...system, attachments: system.attachments ?? [] })),
    worldTexts: Array.isArray(candidate.worldTexts) ? candidate.worldTexts : [],
    magicTexts: Array.isArray(candidate.magicTexts) ? candidate.magicTexts : [],
    writingChapters: normalizeWritingChapters(candidate.writingChapters),
    maps: Array.isArray(candidate.maps) ? candidate.maps : [],
    heatmap: {
      title: "Temperatura narrativa",
      description: "Variables editoriales a lo largo del manuscrito. Puedes cambiar filas, columnas y valores.",
      columnLabels: [...chapterLabels],
    },
    narrativeHeat: state.narrativeHeat.map((row) => {
      const fallback = initialArchive.narrativeHeat.find((item) => item.label === row.label);
      return {
        ...row,
        values: Array.from({ length: chapterLabels.length }, (_, index) => row.values[index] ?? fallback?.values[index] ?? 0),
      };
    }),
  };
}

export function cloneInitialArchive(): ArchiveState {
  return JSON.parse(JSON.stringify(initialArchive)) as ArchiveState;
}

export function createBlankArchive({
  archiveTitle,
  storyTitle,
  theme = "grim",
}: {
  archiveTitle: string;
  storyTitle?: string;
  theme?: ArchiveTheme;
}): ArchiveState {
  const cleanArchiveTitle = archiveTitle.trim() || "Archivo sin título";
  const cleanStoryTitle = storyTitle?.trim() ?? "";

  return {
    dataVersion: 13,
    title: cleanArchiveTitle,
    profile: {
      archiveTitle: cleanArchiveTitle,
      storyTitle: cleanStoryTitle,
      subtitle: "Biblia narrativa editable",
      projectLabel: "Archivo de historia · Proyecto nuevo",
      homeHeading: "Primeras conexiones",
      location: "",
      author: "",
      genre: "",
      status: "Planificación",
      synopsis: "",
      chapterLabels: ["1"],
      theme,
      activeThemeId: theme,
      customThemes: [],
      spotifyPlaylistUrl: "",
      youtubeAmbientUrl: "",
      coverImageDataUrl: "",
      bannerImageDataUrl: "",
      boardIconDataUrl: "",
      boardViewport: { scale: 0.55, x: 42, y: 32, worldSpace: true },
      sceneBoardViewport: { scale: 1, x: 18, y: 18, worldSpace: true },
      sceneBoardCompact: false,
      manuscriptLayout: { ...defaultManuscriptLayout },
      writingAnalysis: { ...defaultWritingAnalysis, fillerPhrases: [...defaultWritingAnalysis.fillerPhrases] },
    },
    manuscript: {
      fileName: "",
      words: 0,
      chapters: 0,
      updatedLabel: "Recién creado",
    },
    characters: [],
    relationships: [],
    theories: [],
    timeline: [],
    world: [],
    magicSystems: [],
    worldTexts: [],
    magicTexts: [],
    writingChapters: [],
    maps: [],
    narrativeHeat: [],
  };
}
