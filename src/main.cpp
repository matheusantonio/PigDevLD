#include "PIG.h"
#include<string.h>
#include <time.h>

PIG_Evento evento;          //evento ser tratadoi a cada pssada do loop principal
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

//#define BASECHAO 280

//#define QTDPISOS 10

// Quantidade de inimigos no cenário
#define NUM_INIMIGOS 1

// Quantidade máxima de caracteres de uma mensagem que pode ser exibida
#define MAX_TEXT_SIZE 100

// Estrutura para armazenar os dados do policial
typedef struct{
    int anima; // Animação do policial
    int timerTeclado; // Timer para as ações do teclado do policial
    int x,y; // Posição no mapa
    int estado; // Estado atual
    int reputacao=0; // Quantidade de reputacao que ele possui
    int direcao; // Direção para a qual ele está olhando (direita ou esquerda)
}Policial;

// Estrutura para armazenar os dados de um inimigo
typedef struct {
    int anima; // Animação do inimigo
    int posX; // Posição do inimigo no mapa
    int posY;
    int timerAtacado; // Timer para definir o intervalo de tempo em que o inimigo pode ser ataco novamente

    int gravidade_crime; // Gravidade do crime do inimigo (negativo para inimigos que não cometem crimes)
    char** mensagem_interacao; // Mensagens exibidas quando o policial interage com o inimigo
    char** mensagem_atacado; // Mensagens exibidas quando o policial ataca o inimigo
    int n_mensagem_interacao; // Quantidade de mensagens de interação
    int n_mensagem_atacado; // Quantidade de mensagens de ataque

} Inimigo;

// Lista encadeada de mensagens a serem exibidas na tela
typedef struct mensagem {
    char* texto; // Texto da mensagem atual
    mensagem *prox;
} Mensagem;

// Função adiciona uma mensagem à lista
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

// Adiciona mais de uma mensagem à lista
void adicionarMensagens(Mensagem **ini, char** text, int n_mensagens){
    for(int i=0;i<n_mensagens;i++){
        adicionarMensagem(ini, text[i]);
    }
}

// Passa para a próxima mensagem
void passarMensagem(Mensagem **ini){
    Mensagem *aux = (*ini)->prox;
    free(*ini);
    *ini = aux;
}


// Armazena o cenário
typedef struct {
    char* nomeArq; // Nome do arquivo de fundo

    int minX, minY, maxX, maxY; // Limites onde o personagem pode andar no cenário

    int largura, altura; // Largura e altura do cenário

} Cenario;

// Função que inicializa o cenário
// Por enquanto, há apenas um cenário. Função será
// otimizada para ler cenários diferentes de acordo
// com a fase em que o jogador está
Cenario inicializarCenario(){
    Cenario cenario;

    cenario.nomeArq = (char*)malloc(62*sizeof(char));
    strcpy(cenario.nomeArq, "cenario_delegacia.png");

    cenario.minX = 170;
    cenario.minY = 130;
    cenario.maxX = 550;
    cenario.maxY = 393;

    cenario.largura = 800;
    cenario.altura = 600;
    // 563, 969
    return cenario;
}

// Declaração da função que testa a colisão com os inimigos
int testaColisaoInimigos(Policial pol, Inimigo inimigos[]);

Inimigo inicializarInimigo() {

    Inimigo inimigo;

    inimigo.anima = CriaAnimacao("..//imagens//inimigos.png", false);
    SetDimensoesAnimacao(inimigo.anima,1.05*ALTPOLICIAL,1.05*LARGPOLICIAL);

    CarregaFramesPorLinhaAnimacao(inimigo.anima, 1, 8, 12);
    CriaModoAnimacao(inimigo.anima, 1, 0);
    InsereFrameAnimacao(inimigo.anima, 1, 2, 0.1);

    DeslocaAnimacao(inimigo.anima, 300, 200);
    inimigo.posX = 300;
    inimigo.posY = 200;
    inimigo.timerAtacado = CriaTimer();

    //Iniciando a gravidade do crime
    inimigo.gravidade_crime = -2;

    inimigo.n_mensagem_atacado = 3;
    inimigo.mensagem_atacado = (char**)malloc(inimigo.n_mensagem_atacado*sizeof(char*));

    for(int i=0;i<inimigo.n_mensagem_atacado;i++){
        inimigo.mensagem_atacado[i] = (char*)malloc(MAX_TEXT_SIZE*sizeof(char));
    }

    strcpy(inimigo.mensagem_atacado[0], "Ai!");
    strcpy(inimigo.mensagem_atacado[1], "Ta maluco?");
    strcpy(inimigo.mensagem_atacado[2], "Sou bandido não, parceiro.");


    inimigo.n_mensagem_interacao = 2;
    inimigo.mensagem_interacao = (char**)malloc(inimigo.n_mensagem_interacao*sizeof(char*));

    for(int i=0;i<inimigo.n_mensagem_interacao;i++){
        inimigo.mensagem_interacao[i] = (char*)malloc(MAX_TEXT_SIZE*sizeof(char));
    }

    strcpy(inimigo.mensagem_interacao[0], "Interagiu com o inimigo!");
    strcpy(inimigo.mensagem_interacao[1], "Segunda interação...");

    MudaModoAnimacao(inimigo.anima, 1, 0);

    return inimigo;
}

// Muda o estado atual do policial
void MudaAcao(Policial &pol,int acao){
    pol.estado = acao;
    MudaModoAnimacao(pol.anima,acao,0);
}

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

//Função para iniciar a Reputação do policial, inserindo um valor aleatório.
void iniciaReputacao(Policial &pol){

    srand(time(NULL));
    // Valor ainda a ser ajustado
    pol.reputacao += rand() % 30 + 0;

}

// Cria o personagem principal
Policial criaPolicial(){
    Policial pol;
    int posX = 600;
    int posY = 600;

    pol.anima = CriaAnimacao("..//imagens//policial.png",0);
    SetDimensoesAnimacao(pol.anima,ALTPOLICIAL,LARGPOLICIAL);

    CarregaFramesPorLinhaAnimacao(pol.anima,1,5,3);

    int k=7;

    // Animação do personagem parado
    CriaModoAnimacao(pol.anima,PARADO,0);
    InsereFrameAnimacao(pol.anima,PARADO,8,0.1);

    // Animação do personagem andando
    CriaModoAnimacao(pol.anima,ANDANDO,1);
    for(int j=0;j<3;j++)
        InsereFrameAnimacao(pol.anima,ANDANDO,k++,0.1);

    // Animação do personagem atacando
    CriaModoAnimacao(pol.anima,ATACANDO,0);
    InsereFrameAnimacao(pol.anima,ATACANDO,13,0.5);

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

void modificaReputacao(Policial &pol, int gravidade_crime){

    // Gerando a seed
    srand(time(NULL));

    int piso= (gravidade_crime-1)*10+1;
    int teto = gravidade_crime * 10;
    pol.reputacao += rand() % teto + piso;

    if(pol.reputacao < 0)
    {
        EscreverEsquerda("Game Over !",50,50);
    }
}

// Função de ataque do policial
void Ataca(Policial &pol, Inimigo inimigos[], Mensagem **mensagemAtual){
    MudaAcao(pol,ATACANDO);

    // Função testa se o ataque acertou algum inimigo
    int inimigoAtingido = testaColisaoInimigos(pol, inimigos);
    if(inimigoAtingido != -1){
        modificaReputacao(pol,inimigos[inimigoAtingido].gravidade_crime);
        printf("\n%d",pol.reputacao);
        // Caso algum inimigo seja atingido, adiciona a mensagem desse inimigo à lista de mensagens
        adicionarMensagens(mensagemAtual, inimigos[inimigoAtingido].mensagem_atacado, inimigos[inimigoAtingido].n_mensagem_atacado);
        ReiniciaTimer(inimigos[inimigoAtingido].timerAtacado);
    }
}

// Função de interação do policial
void Iteracao(Policial &pol, Inimigo inimigos[], Mensagem **mensagemAtual){

    // Função testa se a interação foi feita com algum inimigo
    int inimigoIteracao = testaColisaoInimigos(pol, inimigos);
    if(inimigoIteracao != -1){
        // Caso algum inimigo esteja no alcance, as mensagens dele são adicionadas à lista de mensagens
        adicionarMensagens(mensagemAtual, inimigos[inimigoIteracao].mensagem_interacao, inimigos[inimigoIteracao].n_mensagem_interacao);
    }
}

// Função principal para tratar os eventos de teclado
void TrataEventoTeclado(PIG_Evento ev,Policial &pol, Inimigo inimigos[], Mensagem **mensagemAtual){

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
                Ataca(pol, inimigos, mensagemAtual);
            else if(ev.teclado.tecla == PIG_TECLA_z)
                Iteracao(pol, inimigos, mensagemAtual);
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

void AtualizaPolicial(Policial &pol, Cenario cenario){
    // Atualiza a posição do policial de acordo com o cenário.
    // Essa função impede que o personagem vá além dos limites do cenário.
    int px,py;
    GetXYAnimacao(pol.anima,&px,&py);

    px = PIGLimitaValor(px, cenario.minX, cenario.maxX);
    py = PIGLimitaValor(py,cenario.minY,cenario.maxY);
    pol.x = px;
    pol.y = py;
    MoveAnimacao(pol.anima,px,py);
}

void Direcao(Policial pol){
    int px,py;
    GetXYAnimacao(pol.anima,&px,&py);
    if(px>0){
        pol.direcao = 0;
    }else if(px<0)
        pol.direcao = 1;
}

// Verifica se ocorreu colisão entre o policial e algum dos inimigos.
int testaColisaoInimigos(Policial pol, Inimigo inimigos[]) {
    // Essa função leva em consideração a direção do jogador, por exemplo, se ele estiver
    // de costas mas houver colisão, o ataque e a interação não funcionam.
    Direcao(pol);
    for(int i=0;i<NUM_INIMIGOS;i++) {
        if(TestaColisaoAnimacoes(pol.anima, inimigos[i].anima) && TempoDecorrido(inimigos[i].timerAtacado)>1.0 ){
            if((pol.direcao && inimigos[i].posX > pol.x + LARGPOLICIAL/2)
            || (!pol.direcao && inimigos[i].posX < pol.x -LARGPOLICIAL/2)){
                return i;
            }
        }
    }
    return -1; // Caso não haja interação com ninguém, retorna -1

}



// Função para criação da cena
int CriaCena(Cenario cenario){

    // Concatena o caminho da pasta de imagens com o nome do arquivo do cenário atual
    char* local = (char*)malloc(80*sizeof(char));
    strcpy(local, "..//imagens//");
    strcat(local, cenario.nomeArq);

    int resp = CriaSprite(local,0);
    SetDimensoesSprite(resp,cenario.altura,cenario.largura);
    return resp;
}

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

void verificaReputacao(Policial &pol){

    if(pol.reputacao < 0){
        EscreverEsquerda("Voce foi exonerado !",10,50);
    }
}

// Função principal
int main( int argc, char* args[] ){

    CriaJogo("Law & Disorder");

    meuTeclado = GetTeclado();

    // Inicializa os elementos principais do jogo, jogador, cenário, inimigos...
    Policial pol = criaPolicial();

    Inimigo inimigos[NUM_INIMIGOS];
    inimigos[0] = inicializarInimigo();

    Mensagem* mensagemAtual = NULL; // Inicializa o jogo sem mensagens

    Cenario cenario = inicializarCenario();

    int cena = CriaCena(cenario);

    //loop principal do jogo
    while(JogoRodando()){

        PreparaCameraMovel();

        evento = GetEvento();

        TrataEventoTeclado(evento,pol, inimigos, &mensagemAtual);

        AtualizaPolicial(pol, cenario);

        AjustaCamera(pol);

        IniciaDesenho();

        DesenhaCenario(cena);

        // Desenha os inimigos
        for(int i=0;i<NUM_INIMIGOS;i++) {
            DesenhaAnimacao(inimigos[i].anima);
        }

        DesenhaPolicial(pol);

        if(mensagemAtual != NULL){ // Se ainda tiver mensagem na lista, exibe na tela
            EscreverEsquerda(mensagemAtual->texto, 10, 50);
        }else{
            verificaReputacao(pol);
        }


        //PreparaCameraFixa();

        EncerraDesenho();
    }

    //o jogo será encerrado
    FinalizaJogo();

    return 0;
}
