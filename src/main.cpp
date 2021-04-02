#include "PIG.h"
#include<string.h>
#include <time.h>

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

#define ID_INIMIGO_RUIM 0
#define ID_INIMIGO_BOM 1

// Quantidade de inimigos no cenário
int NUM_INIMIGOS;

// Quantidade máxima de caracteres de uma mensagem que pode ser exibida
#define MAX_TEXT_SIZE 100


int CENA_INICIAL; // começa 1, seta 0 depois que passarem as primeiras mensagens


// Estrutura para armazenar os dados do policial
typedef struct{
    int anima; // Animação do policial
    int timerTeclado; // Timer para as ações do teclado do policial
    int x,y; // Posição no mapa
    int estado; // Estado atual
    int reputacao; // Quantidade de reputacao que ele possui. Se cair a 0 ou abaixo, o policial é exonerado e o jogo acaba.
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

    int presente;
    int nocauteado; // faz o azul aparecer quando essa variavel do vermelho for 1
    int hp;

} Inimigo;



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
    strcpy(cenario.nomeArq, "cenario_delegacia_pol.png");

    cenario.minX = 170;
    cenario.minY = 130;
    cenario.maxX = 550;
    cenario.maxY = 393;

    cenario.largura = 800;
    cenario.altura = 600;
    // 563, 969
    return cenario;
}

/*
 * Estrutura que compõe a fase que está sendo jogada.
 * Contém os inimigos que se encontram na fase, o cenário e
 * armazena as mensagens que serão exibidas pelo sistema na fase.
*/
typedef struct {
    Inimigo *inimigos;
    int n_inimigos;
    Cenario cenario;

    int n_mensagens_iniciais;
    char** mensagens_iniciais;

} Fase;

// Protótipo da função que testa a colisão com os inimigos.
// Função está sendo implementada mais abaixo.
int testaColisaoInimigos(Policial pol, Inimigo inimigos[]);


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
    pol.reputacao += rand() % 50 + 20;

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

    int piso= (gravidade_crime-1)*10+1;
    int teto = gravidade_crime * 10;
    pol.reputacao += rand() % teto + piso;
}

/*
 * Função que trata o ataque do policial, verificando sua interação com os inimigos
 * e modificando o estado de ambos, alem de acionar as mensagens do sistema e falas.
 */
void Ataca(Policial &pol, Inimigo inimigos[], Mensagem **mensagemAtual){
    MudaAcao(pol,ATACANDO);

    // Função testa se o ataque acertou algum inimigo
    int inimigoAtingido = testaColisaoInimigos(pol, inimigos);
    if(inimigoAtingido != -1){
        SetColoracaoAnimacao(inimigos[inimigoAtingido].anima, VERMELHO);
        modificaReputacao(pol,inimigos[inimigoAtingido].gravidade_crime);
        //printf("\n%d",pol.reputacao);
        inimigos[inimigoAtingido].hp --;
        if(inimigos[inimigoAtingido].hp == 0){
            inimigos[inimigoAtingido].nocauteado = 1;
            inimigos[inimigoAtingido].presente = 0;
            adicionarMensagens(mensagemAtual, inimigos[inimigoAtingido].mensagem_atacado, inimigos[inimigoAtingido].n_mensagem_atacado);
        }
        // Caso algum inimigo seja atingido, adiciona a mensagem desse inimigo à lista de mensagens
        ReiniciaTimer(inimigos[inimigoAtingido].timerAtacado);
    }
}

// Função de testa a interação do policia.l
void Interacao(Policial &pol, Inimigo inimigos[], Mensagem **mensagemAtual){

    // Função testa se a interação foi feita com algum inimigo
    int inimigoInteracao = testaColisaoInimigos(pol, inimigos);
    if(inimigoInteracao != -1){
        // Caso algum inimigo esteja no alcance, as mensagens dele são adicionadas à lista de mensagens
        adicionarMensagens(mensagemAtual, inimigos[inimigoInteracao].mensagem_interacao, inimigos[inimigoInteracao].n_mensagem_interacao);
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
                Interacao(pol, inimigos, mensagemAtual);
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
int testaColisaoInimigos(Policial pol, Inimigo inimigos[]) {
    // Essa função leva em consideração a direção do jogador, por exemplo, se ele estiver
    // de costas mas houver colisão, o ataque e a interação não funcionam.
    Direcao(pol);
    for(int i=0;i<NUM_INIMIGOS;i++) {
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
int CriaCena(Cenario cenario){

    // Concatena o caminho da pasta de imagens com o nome do arquivo do cenário atual
    char* local = (char*)malloc(80*sizeof(char));
    strcpy(local, "..//imagens//");
    strcat(local, cenario.nomeArq);

    int resp = CriaSprite(local,0);
    SetDimensoesSprite(resp,cenario.altura,cenario.largura);
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

/*
 * Criação de um dos inimigos para fins de tutorial
 * Função a ser melhorada para o projeto final.
 */
Inimigo criaInimigoBom(){
    // Inimigo Bom
    Inimigo inimigoBom;
    inimigoBom.id_inimigo = ID_INIMIGO_BOM;

    inimigoBom.anima = CriaAnimacao("..//imagens//boneco_bom.png", false);

    SetDimensoesAnimacao(inimigoBom.anima,1.05*ALTPOLICIAL,1.05*LARGPOLICIAL);

    CarregaFramesPorLinhaAnimacao(inimigoBom.anima, 1, 4, 3);
    CriaModoAnimacao(inimigoBom.anima, 1, 0);
    InsereFrameAnimacao(inimigoBom.anima, 1, 2, 0.1);

    DeslocaAnimacao(inimigoBom.anima, 300, 300);
    inimigoBom.x = 300;
    inimigoBom.y = 300;
    inimigoBom.timerAtacado = CriaTimer();

    //Iniciando a gravidade do crime
    inimigoBom.gravidade_crime = -2;
    inimigoBom.presente = 1;
    inimigoBom.nocauteado = 0;
    inimigoBom.hp = 3;


    inimigoBom.n_mensagem_atacado = 9;
    inimigoBom.mensagem_atacado = (char**)malloc(inimigoBom.n_mensagem_atacado*sizeof(char*));

    for(int i=0;i<inimigoBom.n_mensagem_atacado;i++){
        inimigoBom.mensagem_atacado[i] = (char*)malloc(MAX_TEXT_SIZE*sizeof(char));
    }

    strcpy(inimigoBom.mensagem_atacado[0], "[Como pode ver, atacar pessoas que não estão cometendo delito algum]");
    strcpy(inimigoBom.mensagem_atacado[1], "[fará com que você perca a moral na corporação.]");
    strcpy(inimigoBom.mensagem_atacado[2], "[Em caso de reincidência, você pode até ser exonerado.]");
    strcpy(inimigoBom.mensagem_atacado[3], "[Temos uma equipe de paramédicos nas ruas para que evitar que as pessoas morram,]");
    strcpy(inimigoBom.mensagem_atacado[4], "[mas isso não atenuará seu comportamento fora dos princípios da corporação.]");
    strcpy(inimigoBom.mensagem_atacado[5], "[Portanto, tome cuidado.]");
    strcpy(inimigoBom.mensagem_atacado[6], "- Muito bem, novato!");
    strcpy(inimigoBom.mensagem_atacado[7], "- Nós perdoaremos seu comportamento dessa vez, visto que você está apenas começando.");
    strcpy(inimigoBom.mensagem_atacado[8], "- Mas fique ciente de que não haverá uma terceira chance.");

    inimigoBom.n_mensagem_interacao = 2;
    inimigoBom.mensagem_interacao = (char**)malloc(inimigoBom.n_mensagem_interacao*sizeof(char*));

    for(int i=0;i<inimigoBom.n_mensagem_interacao;i++){
        inimigoBom.mensagem_interacao[i] = (char*)malloc(MAX_TEXT_SIZE*sizeof(char));
    }

    strcpy(inimigoBom.mensagem_interacao[0], "- Droga! Acabei de voltar de uma entrevista de emprego e fui reprovado.");
    strcpy(inimigoBom.mensagem_interacao[1], "- Talvez eles não tenham gostado quando eu falei que morava em um bairro pobre.");

    MudaModoAnimacao(inimigoBom.anima, 1, 0);

    return inimigoBom;
}

/* Criação de um dos inimigos para fins de tutoral.
 * Função a ser melhorada para o projeto final.
 */
Inimigo criaInimigoRuim(){
    // Inimigo Ruim
    Inimigo inimigoRuim;
    inimigoRuim.id_inimigo = ID_INIMIGO_RUIM;

    inimigoRuim.anima = CriaAnimacao("..//imagens//boneco_ruim.png", false);

    SetDimensoesAnimacao(inimigoRuim.anima,1.05*ALTPOLICIAL,1.05*LARGPOLICIAL);

    CarregaFramesPorLinhaAnimacao(inimigoRuim.anima, 1, 4, 3);
    CriaModoAnimacao(inimigoRuim.anima, 1, 0);
    InsereFrameAnimacao(inimigoRuim.anima, 1, 2, 0.1);

    DeslocaAnimacao(inimigoRuim.anima, 300, 200);
    inimigoRuim.x = 300;
    inimigoRuim.y = 200;
    inimigoRuim.timerAtacado = CriaTimer();

    //Iniciando a gravidade do crime
    inimigoRuim.gravidade_crime = 2;
    inimigoRuim.presente = 1;
    inimigoRuim.nocauteado = 0;
    inimigoRuim.hp = 3;


    inimigoRuim.n_mensagem_atacado = 8;
    inimigoRuim.mensagem_atacado = (char**)malloc(inimigoRuim.n_mensagem_atacado*sizeof(char*));

    for(int i=0;i<inimigoRuim.n_mensagem_atacado;i++){
        inimigoRuim.mensagem_atacado[i] = (char*)malloc(MAX_TEXT_SIZE*sizeof(char));
    }

    strcpy(inimigoRuim.mensagem_atacado[0], "- Muito bem, novato!");
    strcpy(inimigoRuim.mensagem_atacado[1], "- Aqui na corporação nós respeitamos aqueles que conseguem punir de forma direta, sem rodeios!");
    strcpy(inimigoRuim.mensagem_atacado[2], "- Continue assim que logo você ganhará sua primeira medalha!");
    strcpy(inimigoRuim.mensagem_atacado[3], "- No entanto, a rua não consiste apenas de maus elementos.");
    strcpy(inimigoRuim.mensagem_atacado[4], "- Há pessoas também que estão ali apenas exercendo seus direitos e deveres como cidadãos.");
    strcpy(inimigoRuim.mensagem_atacado[5], "- Tentar uma abordagem e punir essas pessoas pode lhe causar problemas futuros com a chefia.");
    strcpy(inimigoRuim.mensagem_atacado[6], "- Tome cuidado para não hostilizar essas pessoas de forma recorrente.");
    strcpy(inimigoRuim.mensagem_atacado[7], "[Interaja com o outro boneco e discipline-o!]");

    inimigoRuim.n_mensagem_interacao = 3;
    inimigoRuim.mensagem_interacao = (char**)malloc(inimigoRuim.n_mensagem_interacao*sizeof(char*));

    for(int i=0;i<inimigoRuim.n_mensagem_interacao;i++){
        inimigoRuim.mensagem_interacao[i] = (char*)malloc(MAX_TEXT_SIZE*sizeof(char));
    }

    strcpy(inimigoRuim.mensagem_interacao[0], "Aaaaaaaa eu vou matar todo mundo!!!");
    strcpy(inimigoRuim.mensagem_interacao[1], "Cadê a minha faca de cortar manteiga???");
    strcpy(inimigoRuim.mensagem_interacao[2], "Vou rancar seu bucho fora!!!");

    MudaModoAnimacao(inimigoRuim.anima, 1, 0);

    return inimigoRuim;
}

/*
 * Função responsável por instanciar todos os elementos da fase tutorial, como cenário e inimigos.
 */
Fase iniciarTutorial(){

    Cenario cenario;

    cenario.nomeArq = (char*)malloc(62*sizeof(char));
    strcpy(cenario.nomeArq, "cenario_delegacia_pol.png");

    cenario.minX = 170;
    cenario.minY = 130;
    cenario.maxX = 550;
    cenario.maxY = 393;

    cenario.largura = 800;
    cenario.altura = 600;

    Fase fase;

    fase.n_inimigos = 2;
    fase.inimigos = (Inimigo*)malloc(fase.n_inimigos*sizeof(Inimigo));

    fase.inimigos[0] = criaInimigoRuim();
    fase.inimigos[1] = criaInimigoBom();

    fase.inimigos[1].presente = 0;

    fase.cenario = cenario;

    fase.n_mensagens_iniciais = 15;
    fase.mensagens_iniciais = (char**)malloc(fase.n_mensagens_iniciais*sizeof(char*));

    for(int i=0;i<fase.n_mensagens_iniciais;i++){
        fase.mensagens_iniciais[i] = (char*)malloc(MAX_TEXT_SIZE*sizeof(char));
    }

    strcpy(fase.mensagens_iniciais[0], "[Aperte Z para passar as mensagens.]");
    strcpy(fase.mensagens_iniciais[1], "A história se inicia com um policial, após receber sua licença para trabalhar, recebendo");
    strcpy(fase.mensagens_iniciais[2], "instruções do seu superior sobre as principais atividades que ele deverá exercer nas ruas.");
    strcpy(fase.mensagens_iniciais[3], "- Olá, novato! Bem-vindo à corporação. Você será designado para fazer serviços das ruas da cidade,");
    strcpy(fase.mensagens_iniciais[4], "buscando sempre manter a ordem e tendo certeza que os infratores não saiam impunes.");
    strcpy(fase.mensagens_iniciais[5], "- Lembre-se que, desde já, suas atitudes estarão sendo julgadas,");
    strcpy(fase.mensagens_iniciais[6], "então tenha bastante cuidado em suas tomadas de decisão em suas abordagens.");
    strcpy(fase.mensagens_iniciais[7], "- Como um policial em serviço, você deve manter a lei e a ordem nas ruas.");
    strcpy(fase.mensagens_iniciais[8], "- Lembre-se que, no caso de presenciar um delito, você deve punir o infrator para");
    strcpy(fase.mensagens_iniciais[9], "que ele aprenda e que ele tenha ciência das consequências caso isso volte a se repetir.");
    strcpy(fase.mensagens_iniciais[10], "- Antes de ir pras ruas, vamos simular aqui um flagrante.");
    strcpy(fase.mensagens_iniciais[11], "- Colocamos um boneco de treino para que você possa treinar sua abordagem.");
    strcpy(fase.mensagens_iniciais[12], "- Esse boneco de treino está fazendo coisas ruins, então você precisa impedi-lo. Vai!");
    strcpy(fase.mensagens_iniciais[13], "- Apesar de nem sempre funcionar, uma abordagem mais amena pode evitar que algum engano seja cometido.");
    strcpy(fase.mensagens_iniciais[14], "[Aperte Z para interagir com o boneco. Aperte X para atacar o boneco.]");

    return fase;
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
void trataEventosFase(Fase &fase, Mensagem **mensagem, int &fimJogo){

    Inimigo* inimigoBom;
    Inimigo* inimigoRuim;

    int flagBom = 0;
    int flagRuim = 0;

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
        if(flagBom && flagRuim) break;
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
    if(inimigoRuim->hp == 0 && inimigoBom->hp == 0){
        fimJogo = 1;
    }
}


// Função principal
int main( int argc, char* args[] ){

    CriaJogo("Law & Disorder");
    int fimJogo = 0;

    meuTeclado = GetTeclado();

    int fadeTimer = CriaTimer(1);

    // Inicializa os elementos principais do jogo, jogador, cenário, inimigos...
    Policial pol = criaPolicial();
    InsereTransicaoAnimacao(pol.anima, 3, 0, 0, 0, 0, 0, BRANCO, -255);

    char caminhoFonte[100] = "..//fontes//LEMONMILK-Regular.otf";

    int fonte = CriaFonteNormal(caminhoFonte, 10, AMARELO);

    Fase fase_tutorial = iniciarTutorial();

    NUM_INIMIGOS = fase_tutorial.n_inimigos;

    Inimigo* inimigos = fase_tutorial.inimigos;

    Mensagem* mensagemAtual = NULL; // Inicializa o jogo sem mensagens

    // Adiciona mensagens iniciais
    for(int i=0;i<fase_tutorial.n_mensagens_iniciais;i++){
        adicionarMensagem(&mensagemAtual, fase_tutorial.mensagens_iniciais[i]);
    }

    //Cenario cenario = inicializarCenario();

    int cena = CriaCena(fase_tutorial.cenario);
    InsereTransicaoSprite(cena, 3, 0, 0, 0, 0, 0, BRANCO, -255);

    char mensagemFinal[50] = "";

    //loop principal do jogo
    while(JogoRodando()){

        PreparaCameraMovel();

        evento = GetEvento();

        TrataEventoTeclado(evento,pol, inimigos, &mensagemAtual);

        trataEventosFase(fase_tutorial, &mensagemAtual, fimJogo);

        AtualizaPolicial(pol, fase_tutorial.cenario);

        AjustaCamera(pol);

        TrataAutomacaoSprite(cena);
        TrataAutomacaoAnimacao(pol.anima);

        IniciaDesenho();

        DesenhaCenario(cena);

        //EliminaApagados(inimigos, NUM_INIMIGOS);

        // Desenha os inimigos e o policial
        int flag = 0;

        qsort(inimigos, NUM_INIMIGOS, sizeof(Inimigo), ComparaPosicao);

        for(int i=0;i<NUM_INIMIGOS;i++) {
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

        //if(!fimJogo) fimJogo = verificaReputacao(pol, &mensagemAtual);

        if(mensagemAtual != NULL){ // Se ainda tiver mensagem na lista, exibe na tela
            EscreverCentralizada(mensagemAtual->texto, 400, 50, AMARELO, fonte);
        }
        EscreverCentralizada(mensagemFinal, 400, 400, AMARELO);

        EncerraDesenho();

        if(fimJogo && mensagemAtual == NULL) {
            IniciaAutomacaoSprite(cena);
            IniciaAutomacaoAnimacao(pol.anima);
            DespausaTimer(fadeTimer);
            if (TempoDecorrido(fadeTimer) > 3)
                strcpy(mensagemFinal, "Fim do Tutorial!");
            if (TempoDecorrido(fadeTimer) > 10) {
                break;
                //FinalizaJogo();
            }
        }
    }

    //o jogo será encerrado
    FinalizaJogo();

    return 0;
}


