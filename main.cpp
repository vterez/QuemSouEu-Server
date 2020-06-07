#include <SFML/Network.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <unordered_map>
#include <atomic>
#include <set>
#include <mutex>

using namespace sf;
using namespace std;

set<int> ids;
unsigned short porta = 1000;

TcpListener novaconexao;
SocketSelector atividade;
atomic<bool> running=true;
bool jogando=false;
int maximo=0;
std::mutex trava;
unsigned int nomesrecebidos;
int rollatual=1;
int novo_id()
{
    for(int i=0; i<maximo; i++)
    {
        if(ids.find(i)==ids.end())
        {
            ids.insert(i);
            return i;
        }
    }
    ids.insert(maximo);
    return maximo++;
}
class Cliente
{
public:
    TcpSocket soquete;
    int tentativas=0;
    std::wstring nome,nomejogo;
};
unordered_map<int,unique_ptr<Cliente>> clientes;
vector<unordered_map<int,unique_ptr<Cliente>>::iterator> remover;
void checaconexao()
{
    int posi;
    while(running)
    {
        posi=0;
        sleep(seconds(10.f));
        trava.lock();
        for(auto x=clientes.begin(); x!=clientes.end(); ++x)
        {
            x->second->tentativas++;
            if(x->second->tentativas>3)
                remover.push_back(x);
            posi++;
        }
        bool removeu=false;
        for(auto x:remover)
        {
            atividade.remove(x->second->soquete);
            ids.erase(x->first);
            if(x->first+1==maximo)
                maximo--;
            clientes.erase(x);
            removeu=true;
        }
        if(removeu&&clientes.size()==0)
        {
            running=false;
            trava.unlock();
            return;
        }
        trava.unlock();
        remover.clear();
    }
}
void arrumapacote(Packet &pacotein,int id)
{
    int tipo,dest;
    pacotein>>tipo;
    switch(tipo)
    {
    case 1:
    {
        //recebe papel
        pacotein>>dest;
        wstring s;
        pacotein>>s;
        clientes[dest]->nomejogo=s;
        nomesrecebidos++;
        break;
    }
    case 3:
    {
        //começa
        if(nomesrecebidos==clientes.size())
        {
            sf::Packet pctnomes;
            wstring s;
            unsigned int csize=clientes.size();
            pctnomes<<0<<csize;
            for(auto& [k,v]:clientes)
            {
                s=v->nomejogo;
                int i=k;
                pctnomes<<i<<s;
            }
            for(auto& [k,v]:clientes)
                v->soquete.send(pctnomes);
        }
        break;
    }
    case 2:
    {
        //pede nomes
        int i=size(clientes);
        if(i>1)
        {
            sf::Packet pct;
            pct<<1<<rollatual;
            rollatual++;
            if(rollatual==i)
                rollatual=1;
            for(auto& [k,v]:clientes)
                v->soquete.send(pct);
            nomesrecebidos=0;
            jogando=true;
        }
        break;
    }
    case 5:
    {
        //confirmacao
        auto itenvio=clientes.find(id);
        itenvio->second->tentativas-=3;
        if(itenvio->second->tentativas<0)
            itenvio->second->tentativas=0;
        break;
    }
    case 6:
    {
        //finaliza
        clientes[id]->tentativas=15;
        break;
    }
    case 8:
    {
        //pausa pra recomeçar
        jogando=false;
    }
    default:
    {
        break;
    }
    }
}
void checasocket()
{
    if(atividade.isReady(novaconexao))
    {
        if(!jogando)
        {
            auto novocliente=make_unique<Cliente>();
            if(novaconexao.accept(novocliente->soquete)==Socket::Done)
            {
                trava.lock();
                if(!running)
                {
                    trava.unlock();
                    return;
                }
                atividade.add(novocliente->soquete);
                sf::Packet pacote,pacote2,pacote3,pacote4;
                int id=novo_id();
                pacote<<-2<<id;
                novocliente->soquete.send(pacote);
                novocliente->soquete.receive(pacote2);
                std::wstring n;
                pacote2>>n;
                novocliente->nome=n;
                unsigned int csize=clientes.size();
                pacote3<<7<<csize;
                pacote4<<7<<1<<id<<n;
                for(auto &a:clientes)
                {
                    pacote3<<a.first<<a.second->nome;
                    a.second->soquete.send(pacote4);
                }
                novocliente->soquete.send(pacote3);
                clientes[id]=std::move(novocliente);
                trava.unlock();
                return;
            }
        }
        else
        {
            TcpSocket soq;
            if(novaconexao.accept(soq)==Socket::Done)
            {
                Packet pct;
                pct<<-1;
                soq.send(pct);
            }
        }
    }
    //cout<<"procou no cliente\n";
    trava.lock();
    if(!running)
    {
        trava.unlock();
        return;
    }
    for(auto &x:clientes)
    {
        if(atividade.isReady(x.second->soquete))
        {
            Packet pacote,pacote2;
            if(x.second->soquete.receive(pacote) == Socket::Done)
            {
                arrumapacote(pacote,x.first);
            }
        }
    }
    trava.unlock();
}
int main()
{
    try
    {
        novaconexao.listen(porta);
        atividade.add(novaconexao);
        cout<<IpAddress::getLocalAddress()<<endl;
        cout<<IpAddress::getPublicAddress()<<endl;
        thread t1(checaconexao);
        t1.detach();
        while(running)
        {
            if(atividade.wait(seconds(5.f)))
            {
                checasocket();
            }
        }
    }
    catch(exception &e)
    {
        cout<<e.what()<<endl;
    }
    sleep(seconds(3.f));
    return 0;
}
