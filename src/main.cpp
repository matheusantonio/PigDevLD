#include "PIG.h"
#include <stdio.h>
#include<string.h>
#include <time.h>
#include "cJSON.h"

PIG_Evento evento;          //evento ser tratado a cada pssada do loop principal
PIG_Teclado meuTeclado;     //variável como mapeamento do teclado

// Constantes para os estados possívels do personagem
#define PARADO 0
#define ANDANDO 1
#define ATACANDO 2
#define DIALOGO 3

// Definem a altura e a largura do sprite do policial que será desenhado
#define ALTPOLICIAL 48
#define LARGPOLICIAL 48

#define FOLGATELAX 200
#define FOLGATELAY 100

// DEFINES das fases
#define FASE_TUTORIAL 0
#define FASE_RUA 1
#define FASE_BECO 2

// Quantidade máxima de caracteres de uma mensagem que pode ser exibida
#define MAX_TEXT_SIZE 150


int CENA_INICIAL; // começa 1, seta 0 depois que passarem as primeiras mensagens


// Estrutura para armazenar os dados do policial
typedef struct{
    int anima; // Animação do policial
    int timerTeclado; // Timer para as ações do teclado do policial
    int x,y; // Posição no mapa
    int estado; // Estado atual
    int reputacao; // Quantidade de reputacao que ele possui. Se cair a 0 ou abaixo, o policial é exonerado e o jogo acaba.
    int reputacaoInicial;
    int direcao; // Direção para a qual ele está olhando (direita ou esquerda)
}Policial;

// Estrutura para armazenar os dados de um inimigo
typedef struct {

    int id_inimigo;

    int anima; // Animação do inimigo
    int x; // Posição do inimigo no mapa
    int y;
    int timerAtacado; // Timer para definir o intervalo de tempo em que o inimigo pode ser ataco novamente

    int gravidade_crime; // Gravidade do crime do inimigo (negativo para inimigos que não cometem crimes)

    char** mensagem_interacao; // Mensagens exibidas quando o policial interage com o inimigo
    char** mensagem_atacado; // Mensagens exibidas quando o policial ataca o inimigo

    int n_mensagem_interacao; // Quantidade de mensagens de interação
    int n_mensagem_atacado; // Quantidade de mensagens de ataque

    int atacavel;

    int presente;
    int nocauteado; // faz o azul aparecer quando essa variavel do vermelho for 1
    int interagido;
    int hp;

} Inimigo;

// Funções JSON
char * lerArquivoJSON(char *nomeArq){
    long len;
    char *data;

    FILE *f=fopen(nomeArq,"rb");
    fseek(f,0,SEEK_END);
    len=ftell(f);
    fseek(f,0,SEEK_SET);
    data=(char*)malloc(len+1);
    fread(data,1,len,f);
    data[len]='\0';

    return data;
}

// Lista encadeada de mensagens a serem exibidas na tela.
typedef struct mensagem {
    char* texto; // Texto da mensagem atual
    mensagem *prox;
} Mensagem;

// Função que adiciona uma mensagem à lista para ser lida.
void adicionarMensagem(Mensagem **ini, char* texto){
    Mensagem *nova;
    nova = (Mensagem*)malloc(sizeof(Mensagem));
    nova->texto = texto;
    nova->prox = NULL;

    if(*ini == NULL){
        *ini = nova;
    } else{
        Mensagem *aux = *ini;
        while(aux->prox != NULL){
            aux = aux->prox;
        }
        aux->prox = nova;
    }
}

// Função que adiciona mais de uma mensagem à lista.
void adicionarMensagens(Mensagem **ini, char** text, int n_mensagens){
    for(int i=0;i<n_mensagens;i++){
        adicionarMensagem(ini, text[i]);
    }
}

// Função que coloca a próxima mensagem na cabeça da lista encadeada para ser lida.
void passarMensagem(Mensagem **ini){
    Mensagem *aux = (*ini)->prox;
    free(*ini);
    *ini = aux;
}

// Estrutura que compõe o cenário. Contém as principais informações sobre o cenário.
typedef struct {
    char* nomeArq; // Nome do arquivo de fundo

    char * sobreposicao;

    int cena;
    int sobreposto;

    int minX, minY, maxX, maxY; // Limites onde o personagem pode andar no cenário

    int largura, altura; // Largura e altura do cenário

} Cenario;

/*
 * Estrutura que compõe a fase que está sendo jogada.
 * Contém os inimigos que se encontram na fase, o cenário e
 * armazena as mensagens que serão exibidas pelo sistema na fase.
*/
typedef struct {
    Inimigo *inimigos;
    int idFase;
    int n_inimigos;
    Cenario cenario;

    int n_mensagens_iniciais;
    char** mensagens_iniciais;

    int n_mensagens_promovido;
    char** mensagens_promovido;

    int n_mensagens_neutro;
    char** mensagens_neutro;

} Fase;

// Protótipo da função que testa a colisão com os inimigos.
// Função está sendo implementada mais abaixo.
int testaColisaoInimigos(Policial pol, Inimigo inimigos[], int numInimigos);

// Faz o tratamento do estado do policial entre andar e atacar.
void MudaAcao(Policial &pol,int acao){
    pol.estado = acao;
    MudaModoAnimacao(pol.anima,acao,0);
}

// Função que ajusta as dimensões da câmera com relação ao policial.
void AjustaCamera(Policial pol){
    int cx,cy,bx,by,dx=0;
    GetXYCamera(&cx,&cy);
    GetXYAnimacao(pol.anima,&bx,&by);
    if (bx-cx<FOLGATELAX){
        dx = bx-cx-FOLGATELAX;
    }else if (bx-cx>PIG_LARG_TELA-(FOLGATELAX+LARGPOLICIAL)){
        dx = bx-cx-(PIG_LARG_TELA-(FOLGATELAX+LARGPOLICIAL));
    }

    PreparaCameraMovel();
    DeslocaCamera(dx,0);
}

/* Função para iniciar a Reputação do policial, inserindo um valor aleatório.
 * A aleatoriedade se dá para assegurar que o jogador não tenha uma má conduta
 * de forma indiscriminada.
 */
void iniciaReputacao(Policial &pol){

    srand(time(NULL));
    // Valor ainda a ser ajustado
    int rep = rand() % 51 + 50;
    pol.reputacao = rep;
    pol.reputacaoInicial = rep;

    printf("Inicial: %d\n", pol.reputacao);

}

// Cria o personagem principal
Policial criaPolicial(){
    Policial pol;

    const char * arquivoJsonPolicial = lerArquivoJSON("..//arquivos//policial.json");
    cJSON *jsonPolicial = cJSON_Parse(arquivoJsonPolicial);

    int posX = cJSON_GetObjectItemCaseSensitive(jsonPolicial, "posX")->valueint;
    int posY = cJSON_GetObjectItemCaseSensitive(jsonPolicial, "posY")->valueint;

    pol.anima = CriaAnimacao(cJSON_GetObjectItemCaseSensitive(jsonPolicial, "nomeArq")->valuestring, 0);
    SetDimensoesAnimacao(pol.anima,ALTPOLICIAL,LARGPOLICIAL);

    CarregaFramesPorLinhaAnimacao(pol.anima,1,
        cJSON_GetObjectItemCaseSensitive(jsonPolicial, "linhasFrame")->valueint,
        cJSON_GetObjectItemCaseSensitive(jsonPolicial, "colunasFrame")->valueint);


    cJSON *jsonModosAnimacao = cJSON_GetObjectItemCaseSensitive(jsonPolicial, "modosAnimacao");

    for(int i=0;i<cJSON_GetArraySize(jsonModosAnimacao);i++){

        cJSON * jsonModoAnimacao = cJSON_GetArrayItem(jsonModosAnimacao, i);
        int codigoModo = cJSON_GetObjectItemCaseSensitive(jsonModoAnimacao, "codigoModo")->valueint;
        int loop = cJSON_GetObjectItemCaseSensitive(jsonModoAnimacao, "loop")->valueint;

        CriaModoAnimacao(pol.anima, codigoModo, loop);

        cJSON * jsonFramesAnimacao = cJSON_GetObjectItemCaseSensitive(jsonModoAnimacao, "frames");

        for(int j=0;j<cJSON_GetArraySize(jsonFramesAnimacao);j++){

            cJSON* jsonFrameAnimacao = cJSON_GetArrayItem(jsonFramesAnimacao, j);

            InsereFrameAnimacao(pol.anima, codigoModo,
                cJSON_GetObjectItemCaseSensitive(jsonFrameAnimacao, "codigoFrame")->valueint,
                cJSON_GetObjectItemCaseSensitive(jsonFrameAnimacao, "tempo")->valuedouble);
        }

    }

    // Estado inicial do personagem
    pol.estado = PARADO;
    MudaModoAnimacao(pol.anima,PARADO,0);

    // Inicializando a reputação do policial
    iniciaReputacao(pol);

    // Criação do timer e definição do posicioamento do policial
    pol.timerTeclado = CriaTimer();
    pol.y = posY;
    pol.x = posX;

    MoveAnimacao(pol.anima,posX,posY);
    SetFlipAnimacao(pol.anima,PIG_FLIP_HORIZONTAL);
    pol.direcao=0;

    return pol;
}

// Função para alterar a animação e a posição do policial conforme ele se move.
void Move(Policial &pol, int dx, int dy){
    DeslocaAnimacao(pol.anima,dx,dy);
    pol.y += dy;
    if(dx>0){
        SetFlipAnimacao(pol.anima,PIG_FLIP_NENHUM);
        pol.direcao=1;
    }else if(dx<0){
        SetFlipAnimacao(pol.anima,PIG_FLIP_HORIZONTAL);
        pol.direcao=0;
    }
    if (pol.estado == PARADO)
        MudaAcao(pol,ANDANDO);
}

/*
 * Essa função modifica a reputação do policial dependendo da gravidade do crime cometido.
 * Adicionamos um elemento de aleatoriedade para que o jogador não saiba exatamente até onde
 * ele pode ter uma má conduta.
 * Usando a variável gravidade_crime, que varia entre 1 e 5 conforme a gravidade do crime,
 * fazemos com que haja um aumento (ou diminuição, se a gravidade for negativa, ou seja, não
 * há crime) aleatório em um intervalo definido.
*/
void modificaReputacao(Policial &pol, int gravidade_crime){

    // Gerando a seed
    srand(time(NULL));
    int piso= (gravidade_crime-1)*10+1; // 1*10 + 1 11 e 20 -49
    int teto= gravidade_crime * 10; // 31 e 40 -40
    pol.reputacao += rand() % (teto - piso + 1) + piso;

}

/*
 * Função que trata o ataque do policial, verificando sua interação com os inimigos
 * e modificando o estado de ambos, alem de acionar as mensagens do sistema e falas.
 */
void Ataca(Policial &pol, Inimigo inimigos[], Mensagem **mensagemAtual, int numInimigos){
    MudaAcao(pol,ATACANDO);

    // Função testa se o ataque acertou algum inimigo
    int inimigoAtingido = testaColisaoInimigos(pol, inimigos, numInimigos);
    if(!inimigos[inimigoAtingido].atacavel) return;
    if(inimigoAtingido != -1){
        SetColoracaoAnimacao(inimigos[inimigoAtingido].anima, VERMELHO);
        inimigos[inimigoAtingido].hp --;
        if(inimigos[inimigoAtingido].hp == 0){
            modificaReputacao(pol,inimigos[inimigoAtingido].gravidade_crime);
            inimigos[inimigoAtingido].nocauteado = 1;
            inimigos[inimigoAtingido].presente = 0;
            adicionarMensagens(mensagemAtual, inimigos[inimigoAtingido].mensagem_atacado, inimigos[inimigoAtingido].n_mensagem_atacado);
        }
        // Caso algum inimigo seja atingido, adiciona a mensagem desse inimigo à lista de mensagens
        ReiniciaTimer(inimigos[inimigoAtingido].timerAtacado);
    }
}

// Função de testa a interação do policia.l
void Interacao(Policial &pol, Inimigo inimigos[], Mensagem **mensagemAtual, int numInimigos){

    // Função testa se a interação foi feita com algum inimigo
    int inimigoInteracao = testaColisaoInimigos(pol, inimigos, numInimigos);
    if(inimigoInteracao != -1){
        // Caso algum inimigo esteja no alcance, as mensagens dele são adicionadas à lista de mensagens
        inimigos[inimigoInteracao].interagido = 1;
        adicionarMensagens(mensagemAtual, inimigos[inimigoInteracao].mensagem_interacao, inimigos[inimigoInteracao].n_mensagem_interacao);
    }
}

// Função principal para tratar os eventos de teclado
void TrataEventoTeclado(PIG_Evento ev,Policial &pol, Inimigo inimigos[], Mensagem **mensagemAtual, int numInimigos){

    // Caso a lista de mensagens não esteja vazia
    if(*mensagemAtual != NULL){
        if(pol.estado!=ATACANDO) MudaAcao(pol, PARADO); // Se estiver andando quando aperta Z, personagem fica se mexendo como se estivesse andando.
        if(ev.tipoEvento == PIG_EVENTO_TECLADO &&
            ev.teclado.acao == PIG_TECLA_PRESSIONADA &&
            ev.teclado.tecla == PIG_TECLA_z &&
            TempoDecorrido(pol.timerTeclado)>0.5){
            // Caso o jogador aperte Z, a mensagem atual é passada
            passarMensagem(mensagemAtual);
            ReiniciaTimer(pol.timerTeclado);
        }
        return; // Impede que o personagem realiza alguma ação enquanto não passar a mensagem
    }

    // Condição para verificar se o personagem interagiu ou atacou
    if(ev.tipoEvento == PIG_EVENTO_TECLADO && ev.teclado.acao == PIG_TECLA_PRESSIONADA){
       if(pol.estado == PARADO || pol.estado == ANDANDO){
            if(ev.teclado.tecla == PIG_TECLA_x)
                Ataca(pol, inimigos, mensagemAtual, numInimigos);
            else if(ev.teclado.tecla == PIG_TECLA_z)
                Interacao(pol, inimigos, mensagemAtual, numInimigos);
       }
    }
    else if (TempoDecorrido(pol.timerTeclado)>0.005){
        if (meuTeclado[PIG_TECLA_DIREITA]){
            Move(pol,+1,0);
            pol.x+=1;
        }else if (meuTeclado[PIG_TECLA_ESQUERDA]){
            Move(pol,-1,0);
            pol.x-=1;
        }else if (meuTeclado[PIG_TECLA_CIMA]){
            Move(pol,0,+1);
            pol.y+=1;
        }else if (meuTeclado[PIG_TECLA_BAIXO]){
            Move(pol,0,-1);
            pol.y-=1;
        }else{
            if (pol.estado==ANDANDO)
                MudaAcao(pol,PARADO);
        }

        ReiniciaTimer(pol.timerTeclado);
    }
}

void DesenhaPolicial(Policial &pol){
    int aux = DesenhaAnimacao(pol.anima);
    if(pol.estado == ATACANDO && aux==1){
        MudaAcao(pol,PARADO);
    }
}

// Atualiza a posição do policial de acordo com o cenário.
// Essa função impede que o personagem vá além dos limites do cenário.
void AtualizaPolicial(Policial &pol, Cenario cenario){
    int px,py;
    GetXYAnimacao(pol.anima,&px,&py);

    px = PIGLimitaValor(px, cenario.minX, cenario.maxX);
    py = PIGLimitaValor(py,cenario.minY,cenario.maxY);
    pol.x = px;
    pol.y = py;
    MoveAnimacao(pol.anima,px,py);

    printf("Rep: %d | x: %d | y: %d\n", pol.reputacao, pol.x, pol.y);
}

/* Função que trata a direção para qual o policial está virado. */
void Direcao(Policial pol){
    int px,py;
    GetXYAnimacao(pol.anima,&px,&py);
    if(px>0){
        pol.direcao = 0;
    }else if(px<0)
        pol.direcao = 1;
}

// Verifica se ocorreu colisão entre o policial e algum dos inimigos.
int testaColisaoInimigos(Policial pol, Inimigo inimigos[], int numInimigos) {
    // Essa função leva em consideração a direção do jogador, por exemplo, se ele estiver
    // de costas mas houver colisão, o ataque e a interação não funcionam.
    Direcao(pol);
    for(int i=0;i<numInimigos;i++) {
        if(inimigos[i].presente &&
        TestaColisaoAnimacoes(pol.anima, inimigos[i].anima) &&
        TempoDecorrido(inimigos[i].timerAtacado)>1.0 ){
            if((pol.direcao && inimigos[i].x > pol.x + LARGPOLICIAL/2)
            || (!pol.direcao && inimigos[i].x < pol.x -LARGPOLICIAL/2)){
                return i;
            }
        }
    }
    return -1; // Caso não haja interação com ninguém, retorna -1

}


// Função para criação da cena
int CriaCena(char* nomeArq, int altura, int largura){

    // Concatena o caminho da pasta de imagens com o nome do arquivo do cenário atual
    char* local = (char*)malloc((13+strlen(nomeArq))*sizeof(char));
    strcpy(local, "..//imagens//");
    strcat(local, nomeArq);

    int resp = CriaSprite(local,0);
    SetDimensoesSprite(resp,altura,largura);
    return resp;
}

/* Passa como parämetro a cena que deverá ser desenhada */
void DesenhaCenario(int cena){
    MoveSprite(cena,0,0);
    DesenhaSprite(cena);
    // Função de repetição foi deixada comentada porque ainda não
    // sabemos se usaremos no futuro
    /*for (int i=0;i<QTDPISOS;i++){
        MoveSprite(cena,i*PIG_LARG_TELA,0);
        DesenhaSprite(cena);
    }*/
}

/* Função para verificar o valor da reputação do policial.
 * A relação entre a reputação e suas consequências dentro do jogo
 * será trabalhada posteriormente.
*/
int verificaReputacao(Policial &pol, Mensagem **mensagemAtual){

    if(pol.reputacao < 0){
        adicionarMensagem(mensagemAtual, "Voce foi exonerado !");
        return 1;
    }
    return 0;
}

/* Função que ajusta a profundidade das animações para verificar se
 * o desenho de um será feito antes ou depois do outro.
*/
int ComparaPosicao(const void *p1, const void *p2){
    Inimigo ini1 = *(Inimigo*)p1;
    Inimigo ini2 = *(Inimigo*)p2;
    if (ini1.y > ini2.y) return -1;
    if (ini1.y == ini2.y) return 0;
    if (ini1.y < ini2.y) return +1;
}


/* Criação de um dos inimigos para fins de tutoral.
 * Função a ser melhorada para o projeto final.
 */
Inimigo criaInimigoJSON(cJSON* jsonInimigo, int pos){

    Inimigo inimigo;


    inimigo.anima = CriaAnimacao(cJSON_GetObjectItemCaseSensitive(jsonInimigo, "nomeArq")->valuestring, false);
    InsereTransicaoAnimacao(inimigo.anima, 3, 0, 0, 0, 0, 0, BRANCO, -255);

    SetDimensoesAnimacao(inimigo.anima,
        (cJSON_GetObjectItemCaseSensitive(jsonInimigo, "modAltura")->valuedouble)*ALTPOLICIAL,
        (cJSON_GetObjectItemCaseSensitive(jsonInimigo, "modLargura")->valuedouble)*LARGPOLICIAL);

    CarregaFramesPorLinhaAnimacao(inimigo.anima, 1,
        cJSON_GetObjectItemCaseSensitive(jsonInimigo, "linhasFrame")->valueint,
        cJSON_GetObjectItemCaseSensitive(jsonInimigo, "colunasFrame")->valueint);

    cJSON* jsonModosAnimacao = cJSON_GetObjectItemCaseSensitive(jsonInimigo, "modosAnimacao");

    for(int i=0;i<cJSON_GetArraySize(jsonModosAnimacao);i++){

        cJSON * jsonModoAnimacao = cJSON_GetArrayItem(jsonModosAnimacao, i);
        int codigoModo = cJSON_GetObjectItemCaseSensitive(jsonModoAnimacao, "codigoModo")->valueint;
        int loop = cJSON_GetObjectItemCaseSensitive(jsonModoAnimacao, "loop")->valueint;

        CriaModoAnimacao(inimigo.anima, codigoModo, loop);

        cJSON * jsonFramesAnimacao = cJSON_GetObjectItemCaseSensitive(jsonModoAnimacao, "frames");

        for(int j=0;j<cJSON_GetArraySize(jsonFramesAnimacao);j++){

            cJSON* jsonFrameAnimacao = cJSON_GetArrayItem(jsonFramesAnimacao, j);

            InsereFrameAnimacao(inimigo.anima, codigoModo,
                cJSON_GetObjectItemCaseSensitive(jsonFrameAnimacao, "codigoFrame")->valueint,
                cJSON_GetObjectItemCaseSensitive(jsonFrameAnimacao, "tempo")->valuedouble);
        }

    }

    inimigo.x = cJSON_GetObjectItemCaseSensitive(jsonInimigo, "xInicial")->valueint;
    inimigo.y = cJSON_GetObjectItemCaseSensitive(jsonInimigo, "yInicial")->valueint;
    DeslocaAnimacao(inimigo.anima, inimigo.x, inimigo.y);

    inimigo.timerAtacado = CriaTimer();

    inimigo.gravidade_crime = cJSON_GetObjectItemCaseSensitive(jsonInimigo, "gravidadeCrime")->valueint;
    inimigo.presente = cJSON_GetObjectItemCaseSensitive(jsonInimigo, "iniciaPresente")->valueint;
    inimigo.hp = cJSON_GetObjectItemCaseSensitive(jsonInimigo, "hp")->valueint;
    inimigo.nocauteado = 0;
    inimigo.atacavel = cJSON_GetObjectItemCaseSensitive(jsonInimigo, "atacavel")->valueint;
    inimigo.id_inimigo = cJSON_GetObjectItemCaseSensitive(jsonInimigo, "id")->valueint;;
    inimigo.interagido = 0;

    cJSON * jsonMensagensAtacado = cJSON_GetObjectItemCaseSensitive(jsonInimigo, "mensagensAtacado");

    inimigo.n_mensagem_atacado = cJSON_GetArraySize(jsonMensagensAtacado);
    inimigo.mensagem_atacado = (char**)malloc(inimigo.n_mensagem_atacado*sizeof(char*));

    for(int i=0;i<inimigo.n_mensagem_atacado;i++){
        inimigo.mensagem_atacado[i] = (char*)malloc(MAX_TEXT_SIZE*sizeof(char));
        strcpy(inimigo.mensagem_atacado[i], cJSON_GetArrayItem(jsonMensagensAtacado, i)->valuestring);
    }

    cJSON * jsonMensagensInteracao = cJSON_GetObjectItemCaseSensitive(jsonInimigo, "mensagensInteracao");

    inimigo.n_mensagem_interacao = cJSON_GetArraySize(jsonMensagensInteracao);
    inimigo.mensagem_interacao = (char**)malloc(inimigo.n_mensagem_interacao*sizeof(char*));

    for(int i=0;i<inimigo.n_mensagem_interacao;i++){
        inimigo.mensagem_interacao[i] = (char*)malloc(MAX_TEXT_SIZE*sizeof(char));
        strcpy(inimigo.mensagem_interacao[i], cJSON_GetArrayItem(jsonMensagensInteracao, i)->valuestring);
    }

    MudaModoAnimacao(inimigo.anima, 1, 0);

    return inimigo;
}

/*
 * Função responsável por instanciar todos os elementos da fase tutorial, como cenário e inimigos.
 */

void limparFase(Fase &fase){

    free(fase.inimigos);
    free(fase.mensagens_iniciais);

}


Fase carregarFaseJSON(char* arquivoJson, Policial &pol){

    const char * jsonTutorial = lerArquivoJSON(arquivoJson);
    cJSON *tutorial_json = cJSON_Parse(jsonTutorial);

    cJSON *jsonCenario = cJSON_GetObjectItemCaseSensitive(tutorial_json, "cenario");

    pol.x = cJSON_GetObjectItemCaseSensitive(jsonCenario, "xInicial")->valueint;
    pol.y = cJSON_GetObjectItemCaseSensitive(jsonCenario, "yInicial")->valueint;
    MoveAnimacao(pol.anima, pol.x, pol.y);

    Cenario cenario;

    cenario.nomeArq = (char*)malloc(strlen(cJSON_GetObjectItemCaseSensitive(jsonCenario, "nomeArq")->valuestring)*sizeof(char));
    strcpy(cenario.nomeArq,
        cJSON_GetObjectItemCaseSensitive(jsonCenario, "nomeArq")->valuestring);

    cenario.sobreposicao = (char*)malloc(strlen(cJSON_GetObjectItemCaseSensitive(jsonCenario, "sobreposto")->valuestring)*sizeof(char));
    strcpy(cenario.sobreposicao,
        cJSON_GetObjectItemCaseSensitive(jsonCenario, "sobreposto")->valuestring);

    cenario.minX = cJSON_GetObjectItemCaseSensitive(jsonCenario, "minX")->valueint;
    cenario.minY = cJSON_GetObjectItemCaseSensitive(jsonCenario, "minY")->valueint;
    cenario.maxX = cJSON_GetObjectItemCaseSensitive(jsonCenario, "maxX")->valueint;
    cenario.maxY = cJSON_GetObjectItemCaseSensitive(jsonCenario, "maxY")->valueint;

    cenario.largura = cJSON_GetObjectItemCaseSensitive(jsonCenario, "largura")->valueint;
    cenario.altura = cJSON_GetObjectItemCaseSensitive(jsonCenario, "altura")->valueint;

    cJSON *jsonInimigos = cJSON_GetObjectItemCaseSensitive(tutorial_json, "inimigos");

    Fase fase;

    fase.n_inimigos = cJSON_GetArraySize(jsonInimigos);
    fase.inimigos = NULL;
    fase.inimigos = (Inimigo*)malloc(fase.n_inimigos*sizeof(Inimigo));

    for(int i=0;i<fase.n_inimigos;i++){
        fase.inimigos[i] = criaInimigoJSON(cJSON_GetArrayItem(jsonInimigos, i), i);
    }

    fase.cenario = cenario;

    cJSON *jsonMensagens = cJSON_GetObjectItemCaseSensitive(tutorial_json, "mensagensIniciais");

    fase.n_mensagens_iniciais = cJSON_GetArraySize(jsonMensagens);
    fase.mensagens_iniciais = NULL;
    fase.mensagens_iniciais = (char**)malloc(fase.n_mensagens_iniciais*sizeof(char*));

    for(int i=0;i<fase.n_mensagens_iniciais;i++){
        fase.mensagens_iniciais[i] = (char*)malloc(MAX_TEXT_SIZE*sizeof(char));
        strcpy(fase.mensagens_iniciais[i], cJSON_GetArrayItem(jsonMensagens, i)->valuestring);
    }


    // Mensagens promovido
    cJSON *jsonMensagensPromovido = cJSON_GetObjectItemCaseSensitive(tutorial_json, "mensagensPromovido");

    fase.n_mensagens_promovido = cJSON_GetArraySize(jsonMensagensPromovido);
    fase.mensagens_promovido = NULL;
    fase.mensagens_promovido = (char**)malloc(fase.n_mensagens_promovido*sizeof(char*));

    for(int i=0;i<fase.n_mensagens_promovido;i++){
        fase.mensagens_promovido[i] = (char*)malloc(MAX_TEXT_SIZE*sizeof(char));
        strcpy(fase.mensagens_promovido[i], cJSON_GetArrayItem(jsonMensagensPromovido, i)->valuestring);
    }


    // Mensagens neutro
    cJSON *jsonMensagensNeutro = cJSON_GetObjectItemCaseSensitive(tutorial_json, "mensagensNeutro");

    fase.n_mensagens_neutro = cJSON_GetArraySize(jsonMensagensNeutro);
    fase.mensagens_neutro = NULL;
    fase.mensagens_neutro = (char**)malloc(fase.n_mensagens_neutro*sizeof(char*));

    for(int i=0;i<fase.n_mensagens_neutro;i++){
        fase.mensagens_neutro[i] = (char*)malloc(MAX_TEXT_SIZE*sizeof(char));
        strcpy(fase.mensagens_neutro[i], cJSON_GetArrayItem(jsonMensagensNeutro, i)->valuestring);
    }

    return fase;
}

Fase carregarTutorialJSON(Policial &pol){
    Fase faseTutorial = carregarFaseJSON("..//arquivos//tutorial.json", pol);
    faseTutorial.idFase = FASE_TUTORIAL;
    return faseTutorial;

}

Fase carregaRuaJSON(Policial &pol){
    Fase faseRua = carregarFaseJSON("..//arquivos//rua.json", pol);
    faseRua.idFase = FASE_RUA;
    return faseRua;
}

Fase carregaBecoJSON(Policial &pol){
    Fase faseBeco = carregarFaseJSON("..//arquivos//beco.json", pol);
    faseBeco.idFase = FASE_BECO;
    return faseBeco;
}

void EliminaApagados(Inimigo inimigos[],int &qtdInimigos){
    int i=0;

    while (i<qtdInimigos){
        if (inimigos[i].hp==0){
            qtdInimigos--;
            inimigos[i] = inimigos[qtdInimigos];
        }else i++;
    }
}


/*
 * Função que trata de eventos de transição da fase,
 * como o surgimento de inimigos e verificação de fase completada.
 */
void trataEventosTutorial(Fase &fase, Mensagem **mensagem, int &fimJogo, Policial &pol){

    const int ID_INIMIGO_RUIM = 0;
    const int ID_INIMIGO_BOM = 1;
    const int ID_POLICIAL = 2;

    Inimigo* inimigoBom;
    Inimigo* inimigoRuim;
    Inimigo* inimigoPolicial;

    int flagBom = 0;
    int flagRuim = 0;
    int flagPolicial = 0;

    for(int i=0;i<fase.n_inimigos;i++){
        if(fase.inimigos[i].id_inimigo == ID_INIMIGO_BOM){
            inimigoBom = &(fase.inimigos[i]);
            flagBom = 1;
            continue;
        }
        if(fase.inimigos[i].id_inimigo == ID_INIMIGO_RUIM){
            inimigoRuim = &(fase.inimigos[i]);
            flagRuim = 1;
            continue;
        }
        if(fase.inimigos[i].id_inimigo == ID_POLICIAL){
            inimigoPolicial = &(fase.inimigos[i]);
            flagPolicial = 1;
            continue;
        }
        if(flagBom && flagRuim && flagPolicial) break;
    }

    if(TempoDecorrido(inimigoRuim->timerAtacado) > 0.4) {
        SetColoracaoAnimacao(inimigoRuim->anima, BRANCO);
    }
    if(TempoDecorrido(inimigoBom->timerAtacado) > 0.4) {
        SetColoracaoAnimacao(inimigoBom->anima, BRANCO);
    }

    if(inimigoRuim->hp == 0 && inimigoBom->hp > 0){
        inimigoBom->presente = 1;
    }

    if(inimigoPolicial->interagido == 1){

        if(!(inimigoBom->hp == 0)){

            inimigoPolicial->interagido = 0;

        } else if(*mensagem == NULL) {

            Fase novaFase = carregaRuaJSON(pol);
            novaFase.cenario.cena = CriaCena(novaFase.cenario.nomeArq, novaFase.cenario.altura, novaFase.cenario.largura);
            InsereTransicaoSprite(novaFase.cenario.cena, 3, 0, 0, 0, 0, 0, BRANCO, -255);

            novaFase.cenario.sobreposto = CriaCena(novaFase.cenario.sobreposicao, novaFase.cenario.altura, novaFase.cenario.largura);

            fase = novaFase;
        }
    }
}

void trataEventosRua(Fase &fase, Mensagem **mensagem, int &fimJogo, Policial &pol){

    const int ID_POLICIAL = 0;

    Inimigo* inimigoPolicial;

    int flagPolicial = 0;

    for(int i=0;i<fase.n_inimigos;i++){
        if(fase.inimigos[i].id_inimigo == ID_POLICIAL){
            inimigoPolicial = &(fase.inimigos[i]);
            flagPolicial = 1;
            continue;
        }
        if(flagPolicial) break;
    }

    for(int i=0;i<fase.n_inimigos;i++){
        if(TempoDecorrido(fase.inimigos[i].timerAtacado) > 0.4) {
            SetColoracaoAnimacao(fase.inimigos[i].anima, BRANCO);
        }
    }

    for(int i=0;i<fase.n_inimigos;i++){
        TrataAutomacaoAnimacao(fase.inimigos[i].anima);
    }

    if(inimigoPolicial->interagido && *mensagem == NULL){

        if(pol.reputacao == pol.reputacaoInicial){

            adicionarMensagens(mensagem, fase.mensagens_neutro, fase.n_mensagens_neutro);
            inimigoPolicial->interagido = 0;

        } else {



            Fase novaFase = carregaBecoJSON(pol);
            novaFase.cenario.cena = CriaCena(novaFase.cenario.nomeArq, novaFase.cenario.altura, novaFase.cenario.largura);
            InsereTransicaoSprite(novaFase.cenario.cena, 3, 0, 0, 0, 0, 0, BRANCO, -255);

            novaFase.cenario.sobreposto = CriaCena(novaFase.cenario.sobreposicao, novaFase.cenario.altura, novaFase.cenario.largura);
            InsereTransicaoSprite(novaFase.cenario.sobreposto, 3, 0, 0, 0, 0, 0, BRANCO, -255);

            fase = novaFase;
        }
    }
}



void trataEventosBeco(Fase &fase, Mensagem **mensagem, int &fimJogo, Policial &pol){

    int somRisada = CriaAudio("..//audio//risada.wav", 0);
    SetVolume(somRisada, 15);

    int somMacaco = CriaAudio("..//audio//macaco.wav", 0);
    SetVolume(somMacaco, 20);

    const int ID_POLICIAL = 0;
    const int ID_ANTONIO = 6;
    const int ID_MACACO = 2;

    Inimigo* inimigoPolicial;
    Inimigo* inimigoAntonio;
    Inimigo* inimigoMacaco;

    int flagPolicial = 0;
    int flagAntonio = 0;
    int flagMacaco = 0;

    for(int i=0;i<fase.n_inimigos;i++){
        if(fase.inimigos[i].id_inimigo == ID_POLICIAL){
            inimigoPolicial = &(fase.inimigos[i]);
            flagPolicial = 1;
            continue;
        }
        if(fase.inimigos[i].id_inimigo == ID_ANTONIO){
            inimigoAntonio = &(fase.inimigos[i]);
            flagAntonio = 1;
            continue;
        }
        if(fase.inimigos[i].id_inimigo == ID_MACACO){
            inimigoMacaco = &(fase.inimigos[i]);
            flagMacaco = 1;
            continue;
        }
        if(flagPolicial && flagAntonio && flagMacaco) break;
    }

    for(int i=0;i<fase.n_inimigos;i++){
        if(TempoDecorrido(fase.inimigos[i].timerAtacado) > 0.4) {
            SetColoracaoAnimacao(fase.inimigos[i].anima, BRANCO);
        }
    }

    for(int i=0;i<fase.n_inimigos;i++){
        TrataAutomacaoAnimacao(fase.inimigos[i].anima);
    }

    if(inimigoAntonio->interagido){
        PlayAudio(somRisada);
        inimigoAntonio->interagido = 0;
    }

    if(inimigoMacaco->interagido){
        PlayAudio(somMacaco);
        inimigoMacaco->interagido = 0;
    }

    if(inimigoPolicial->interagido && *mensagem == NULL){

        if(pol.reputacao == pol.reputacaoInicial){

            adicionarMensagens(mensagem, fase.mensagens_neutro, fase.n_mensagens_neutro);
            inimigoPolicial->interagido = 0;

        } else {
            fimJogo = 1;
        }
    }
}


void trataEventosFases(Fase &fase, Mensagem **mensagem, int &fimJogo, Policial &pol){
    switch(fase.idFase){
        case FASE_TUTORIAL:
            trataEventosTutorial(fase, mensagem, fimJogo, pol);
            break;
        case FASE_RUA:
            trataEventosRua(fase, mensagem, fimJogo, pol);
            break;
        case FASE_BECO:
            trataEventosBeco(fase, mensagem, fimJogo, pol);
            break;
    }
}

// Função que desenha os inimigos e o policial.
void desenhaPersonagens(Inimigo inimigos[], Policial &pol, int &qtdInimigos){

        int flag = 0;

        qsort(inimigos, qtdInimigos, sizeof(Inimigo), ComparaPosicao);

        for(int i=0;i<qtdInimigos;i++) {
            if(inimigos[i].presente || (inimigos[i].nocauteado && TempoDecorrido(inimigos[i].timerAtacado) < 0.4)){
                if(pol.y>inimigos[i].y && flag!=1){
                    DesenhaPolicial(pol);
                    flag = 1;
                }
                DesenhaAnimacao(inimigos[i].anima);
            }
        }
        if(flag == 0){
            DesenhaPolicial(pol);
        }

}

/*
*  Função para finalizar o tutorial
*/
int jogoFinalizou(int fimJogo, Mensagem *mensagemAtual, Policial &pol,Fase faseAtual, int fadeTimer, char mensagemFinal[]){

    int i;

    if(fimJogo && (mensagemAtual == NULL)) {

            //StopBackground();
            //CarregaBackground("..//audio//proerd.wav");
            //SetVolumeBackground(10);
            //PlayBackground();
            int audioProerd = CriaAudio("..//audio//proerd.wav", 0);
            SetVolume(audioProerd, 1);
            PlayAudio(audioProerd);

            IniciaAutomacaoSprite(faseAtual.cenario.cena);
            IniciaAutomacaoSprite(faseAtual.cenario.sobreposto);
            IniciaAutomacaoAnimacao(pol.anima);
            for(i=0;i<faseAtual.n_inimigos;i++) {
                IniciaAutomacaoAnimacao(faseAtual.inimigos[i].anima);
            }
            DespausaTimer(fadeTimer);
            if (TempoDecorrido(fadeTimer) > 3)
                strcpy(mensagemFinal, "Fim do Jogo!");
            if (TempoDecorrido(fadeTimer) > 10)
                return 1;
    }
     return 0;
}

// Função que escreve as mensagens na tela
void escreveMensagem(Mensagem *mensagemAtual,int fonte,char mensagemFinal[]){

    int x, y;
    GetXYCamera(&x,&y);

    if(mensagemAtual != NULL){ // Se ainda tiver mensagem na lista, exibe na tela
        EscreverCentralizada(mensagemAtual->texto, x+400, 50, AMARELO, fonte);
    }
    EscreverCentralizada(mensagemFinal, x+400, 400, AMARELO);
}

// Função principal
int main( int argc, char* args[] ){

    CriaJogo("Law & Disorder");
    int fimJogo = 0;

    meuTeclado = GetTeclado();

    int fadeTimer = CriaTimer(1);

    CarregaBackground("..//audio//cidade_alerta.mp3");
    SetVolumeBackground(7);

    // Inicializa os elementos principais do jogo, jogador, cenário, inimigos...
    Policial pol = criaPolicial();
    InsereTransicaoAnimacao(pol.anima, 3, 0, 0, 0, 0, 0, BRANCO, -255);

    char caminhoFonte[100] = "..//fontes//LEMONMILK-Regular.otf";

    int fonte = CriaFonteNormal(caminhoFonte, 10, AMARELO, PIG_ESTILO_NEGRITO);

    Fase faseAtual = carregaBecoJSON(pol);//carregarTutorialJSON(pol);//carregaRuaJSON(pol);//

    Mensagem* mensagemAtual = NULL; // Inicializa o jogo sem mensagens

    // Adiciona mensagens iniciais
    for(int i=0;i<faseAtual.n_mensagens_iniciais;i++){
        adicionarMensagem(&mensagemAtual, faseAtual.mensagens_iniciais[i]);
    }

    faseAtual.cenario.cena = CriaCena(faseAtual.cenario.nomeArq, faseAtual.cenario.altura, faseAtual.cenario.largura);
    InsereTransicaoSprite(faseAtual.cenario.cena, 3, 0, 0, 0, 0, 0, BRANCO, -255);

    faseAtual.cenario.sobreposto = CriaCena(faseAtual.cenario.sobreposicao, faseAtual.cenario.altura, faseAtual.cenario.largura);
    InsereTransicaoSprite(faseAtual.cenario.sobreposto, 3, 0, 0, 0, 0, 0, BRANCO, -255);

    char mensagemFinal[50] = "";


    PlayBackground();

    //loop principal do jogo
    while(JogoRodando()){

        PreparaCameraMovel();

        evento = GetEvento();

        TrataEventoTeclado(evento,pol, faseAtual.inimigos, &mensagemAtual, faseAtual.n_inimigos);

        trataEventosFases(faseAtual, &mensagemAtual, fimJogo, pol);

        AtualizaPolicial(pol, faseAtual.cenario);

        AjustaCamera(pol);

        TrataAutomacaoSprite(faseAtual.cenario.cena);
        TrataAutomacaoSprite(faseAtual.cenario.sobreposto);
        TrataAutomacaoAnimacao(pol.anima);

        IniciaDesenho();

        DesenhaCenario(faseAtual.cenario.cena);

        //EliminaApagados(inimigos, NUM_INIMIGOS);

        desenhaPersonagens(faseAtual.inimigos,pol,faseAtual.n_inimigos);

        if(!fimJogo) fimJogo = verificaReputacao(pol, &mensagemAtual);

        DesenhaCenario(faseAtual.cenario.sobreposto);

        escreveMensagem(mensagemAtual,fonte,mensagemFinal);

        //PreparaCameraFixa();

        EncerraDesenho();

        if(jogoFinalizou(fimJogo,mensagemAtual,pol,faseAtual, fadeTimer, mensagemFinal)) break;
    }

    //o jogo será encerrado
    FinalizaJogo();

    return 0;
}


