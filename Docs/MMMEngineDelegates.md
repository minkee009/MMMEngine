# MMMEngine::Delegates

```c++
class GameEventBus
{
private:
    // 1) owner 더미 인스턴스 (정적이지만 '소유자'로 쓸 실제 주소)
    inline static GameEventBus s_owner{};      // C++17 이상
    GameEventBus() = default;                  // 외부 생성 금지 (전역 상태 고정)

public:
    // 2) C#의 static event에 해당하는 static 멤버 이벤트
    inline static Event<GameEventBus, void(int)> ScoreChanged{ &s_owner };

    // 3) C#의 Raise/Invoke에 해당: EventHub만 호출 가능
    static void RaiseScoreChanged(int score)
    {
        ScoreChanged(&s_owner, score); // owner 체크 통과
    }
};
```

-> 정적 클래스에서 이벤트 쓰는법

 





```c++
onMatrixUpdate.AddListener<Listener, &Listener::OnMatrixUpdated>(&li);
```

